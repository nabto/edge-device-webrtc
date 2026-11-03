// Promise based wrappers around the Nabto Edge Client SDK.
//
// All asynchronous SDK operations resolve a NabtoClientFuture. We attach a
// single registered koffi callback to every future. The SDK invokes it from
// its own thread; koffi queues the call onto the JS thread, where we look the
// future up in `pending` and settle the corresponding Promise. Nothing ever
// blocks the JS thread waiting for the SDK.
//
// Memory handed to an asynchronous operation (stream read/write buffers, out
// parameters) must stay valid until the future resolves, so it is allocated
// with koffi.alloc() and freed once the callback has run.

import { EC_OK, FutureCallback, Ptr, errorMessage, errorString, koffi, nc, takeSdkString } from './ffi';

export class NabtoError extends Error {
  /** Error name from nabto_client_error_get_string(), e.g. "EOF", "STOPPED", "NO_CHANNELS". */
  readonly code: string;

  constructor(readonly ec: number, context: string, detail?: string) {
    super(`${context}: ${errorMessage(ec)} (${errorString(ec)})${detail ? `. ${detail}` : ''}`);
    this.code = errorString(ec);
    this.name = 'NabtoError';
  }
}

interface PendingFuture {
  /** The client plus the native objects the operation uses (connection, stream, ...). */
  owners: Ptr[];
  settle: (ec: number) => void;
}

const pending = new Map<bigint, PendingFuture>();
const drainWaiters: Array<() => void> = [];

// Neither a pending Promise nor koffi's cross-thread callback queue keeps the
// Node event loop alive, so hold a timer while any future is outstanding.
// Without it a plain Node process (e.g. the CLI smoke test) can exit early.
let keepAlive: NodeJS.Timeout | undefined;

function trackPending(future: bigint, entry: PendingFuture): void {
  pending.set(future, entry);
  keepAlive ??= setInterval(() => {}, 1 << 30);
}

const futureCallback = koffi.register((future: bigint, ec: number) => {
  const entry = pending.get(future);
  pending.delete(future);
  if (pending.size === 0 && keepAlive) {
    clearInterval(keepAlive);
    keepAlive = undefined;
  }
  // Free the future outside of the SDK callback.
  setImmediate(() => {
    nc.futureFree(future);
    drainWaiters.splice(0).forEach((w) => w());
  });
  entry?.settle(ec);
}, koffi.pointer(FutureCallback));

/// Resolves once every outstanding operation on `owner` has resolved and its
/// future has been freed. Native objects must not be freed before that.
async function drainFutures(owner: Ptr): Promise<void> {
  while ([...pending.values()].some((p) => p.owners.includes(owner))) {
    await new Promise<void>((resolve) => drainWaiters.push(resolve));
  }
  await new Promise((resolve) => setImmediate(resolve));
}

/// Create a future, start an operation on it and resolve with its error code.
/// `owners` are the client and the native objects the operation uses.
function runFuture(owners: Ptr[], start: (future: Ptr) => void): Promise<number> {
  const client = owners[0];
  const future: Ptr = nc.futureNew(client);
  if (!future) {
    return Promise.reject(new Error('nabto_client_future_new failed'));
  }
  return new Promise((resolve) => {
    trackPending(future, { owners, settle: resolve });
    start(future);
    nc.futureSetCallback(future, futureCallback, null);
  });
}

async function runChecked(owners: Ptr[], context: string, start: (future: Ptr) => void): Promise<void> {
  const ec = await runFuture(owners, start);
  if (ec !== EC_OK) {
    throw new NabtoError(ec, context);
  }
}

function checkEc(ec: number, context: string): void {
  if (ec !== EC_OK) {
    throw new NabtoError(ec, context);
  }
}

/// Copy `len` bytes out of native memory into a JS owned Buffer. koffi.view()
/// is not an option: Electron's V8 memory cage forbids ArrayBuffers backed by
/// external memory.
function copyOut(ptr: Ptr, len: number): Buffer {
  if (len === 0) {
    return Buffer.alloc(0);
  }
  const bytes = koffi.decode(ptr, koffi.array('uint8_t', len, 'Typed')) as Uint8Array;
  return Buffer.from(bytes.buffer, bytes.byteOffset, bytes.byteLength);
}

/// Copy `data` into native memory at `ptr`.
function copyIn(ptr: Ptr, data: Uint8Array): void {
  if (data.length > 0) {
    koffi.encode(ptr, koffi.array('uint8_t', data.length, 'Typed'), data);
  }
}

export class NabtoClient {
  readonly ptr: Ptr;
  private freed = false;

  constructor() {
    this.ptr = nc.clientNew();
    if (!this.ptr) {
      throw new Error('nabto_client_new failed');
    }
  }

  static version(): string {
    return nc.version() as string;
  }

  setLogLevel(level: 'error' | 'warn' | 'info' | 'trace'): void {
    checkEc(nc.setLogLevel(this.ptr, level), 'set log level');
  }

  createPrivateKey(): string {
    const out: [Ptr | null] = [null];
    checkEc(nc.createPrivateKey(this.ptr, out), 'create private key');
    return takeSdkString(out[0] as Ptr);
  }

  createConnection(): Connection {
    return new Connection(this);
  }

  createMdnsScanner(): MdnsScanner {
    return new MdnsScanner(this);
  }

  /// Stop all outstanding operations, wait for their futures to resolve and
  /// free the client. Connections, streams and listeners must not be used after this.
  async close(): Promise<void> {
    if (this.freed) {
      return;
    }
    this.freed = true;
    // nabto_client_stop() blocks until all queued SDK callbacks have run. Our
    // callbacks are delivered on the JS thread, so calling it synchronously
    // here would deadlock. Run it on a koffi worker thread instead.
    await new Promise<void>((resolve, reject) => {
      nc.clientStop.async(this.ptr, (err: Error | null) => (err ? reject(err) : resolve()));
    });
    await drainFutures(this.ptr);
    nc.clientFree(this.ptr);
  }
}

export interface ConnectionOptions {
  ProductId: string;
  DeviceId: string;
  PrivateKey: string;
  ServerConnectToken?: string;
  ServerUrl?: string;
}

export type ConnectionType = 'relay' | 'direct';

export interface CoapResponse {
  status: number;
  contentFormat: number | null;
  payload: Buffer;
}

export class Connection {
  readonly ptr: Ptr;
  private freed = false;

  constructor(private readonly client: NabtoClient) {
    this.ptr = nc.connectionNew(client.ptr);
    if (!this.ptr) {
      throw new Error('nabto_client_connection_new failed');
    }
  }

  setOptions(options: Partial<ConnectionOptions>): void {
    // The SDK rejects empty strings for optional values, so leave them out.
    const filtered = Object.fromEntries(Object.entries(options).filter(([, v]) => v !== undefined && v !== ''));
    checkEc(nc.connectionSetOptions(this.ptr, JSON.stringify(filtered)), 'set connection options');
  }

  async connect(): Promise<void> {
    const ec = await runFuture([this.client.ptr, this.ptr], (f) => nc.connectionConnect(this.ptr, f));
    if (ec === EC_OK) {
      return;
    }
    if (errorString(ec) === 'NO_CHANNELS') {
      const local = errorString(nc.connectionGetLocalChannelErrorCode(this.ptr));
      const remote = errorString(nc.connectionGetRemoteChannelErrorCode(this.ptr));
      throw new NabtoError(ec, 'connect', `Local channel: ${local}, remote channel: ${remote}`);
    }
    throw new NabtoError(ec, 'connect');
  }

  passwordAuthenticate(username: string, password: string): Promise<void> {
    return runChecked([this.client.ptr, this.ptr], 'password authentication',
      (f) => nc.connectionPasswordAuthenticate(this.ptr, username, password, f));
  }

  deviceFingerprint(): string {
    const out: [Ptr | null] = [null];
    checkEc(nc.connectionGetDeviceFingerprint(this.ptr, out), 'get device fingerprint');
    return takeSdkString(out[0] as Ptr);
  }

  clientFingerprint(): string {
    const out: [Ptr | null] = [null];
    checkEc(nc.connectionGetClientFingerprint(this.ptr, out), 'get client fingerprint');
    return takeSdkString(out[0] as Ptr);
  }

  /// RELAY means traffic goes through the basestation, DIRECT is a local or
  /// UDP hole punched p2p connection.
  type(): ConnectionType {
    const out = [0];
    checkEc(nc.connectionGetType(this.ptr, out), 'get connection type');
    return out[0] === 0 ? 'relay' : 'direct';
  }

  async coap(method: 'GET' | 'POST' | 'PUT' | 'DELETE', path: string,
             body?: { contentFormat: number; payload: Uint8Array }): Promise<CoapResponse> {
    const coap: Ptr = nc.coapNew(this.ptr, method, path);
    if (!coap) {
      throw new Error('nabto_client_coap_new failed');
    }
    try {
      if (body) {
        checkEc(nc.coapSetRequestPayload(coap, body.contentFormat, Buffer.from(body.payload), body.payload.length),
          `${method} ${path} set payload`);
      }
      await runChecked([this.client.ptr, this.ptr, coap], `${method} ${path}`, (f) => nc.coapExecute(coap, f));
      const status = [0];
      checkEc(nc.coapGetResponseStatusCode(coap, status), 'get coap status');
      const cf = [0];
      const hasCf = nc.coapGetResponseContentFormat(coap, cf) === EC_OK;
      const payloadPtr: [Ptr | null] = [null];
      const payloadLen = [0];
      let payload: Buffer = Buffer.alloc(0);
      // NO_DATA is returned when the response has no payload.
      if (nc.coapGetResponsePayload(coap, payloadPtr, payloadLen) === EC_OK && payloadPtr[0]) {
        payload = copyOut(payloadPtr[0] as Ptr, Number(payloadLen[0]));
      }
      return { status: status[0], contentFormat: hasCf ? cf[0] : null, payload };
    } finally {
      nc.coapFree(coap);
    }
  }

  async openStream(port: number): Promise<Stream> {
    const stream = new Stream([this.client.ptr, this.ptr], nc.streamNew(this.ptr));
    try {
      await stream.open(port);
    } catch (err) {
      await stream.free();
      throw err;
    }
    return stream;
  }

  /// Gracefully close the connection, then stop and free it. Safe to call
  /// more than once.
  async close(): Promise<void> {
    if (this.freed) {
      return;
    }
    this.freed = true;
    try {
      await runFuture([this.client.ptr, this.ptr], (f) => nc.connectionClose(this.ptr, f));
    } finally {
      nc.connectionStop(this.ptr);
      await drainFutures(this.ptr);
      nc.connectionFree(this.ptr);
    }
  }
}

export class Stream {
  private freed = false;
  private readonly owners: Ptr[];

  /// `parents` are the client and connection the stream belongs to.
  constructor(parents: Ptr[], readonly ptr: Ptr) {
    if (!ptr) {
      throw new Error('nabto_client_stream_new failed');
    }
    this.owners = [...parents, ptr];
  }

  open(port: number): Promise<void> {
    return runChecked(this.owners, `open stream on port ${port}`, (f) => nc.streamOpen(this.ptr, f, port));
  }

  /// Read exactly `length` bytes. Rejects with code "EOF" when the device closed the stream.
  async readAll(length: number): Promise<Buffer> {
    const buffer: Ptr = koffi.alloc('uint8_t', Math.max(length, 1));
    const readLength: Ptr = koffi.alloc('size_t', 1);
    try {
      await runChecked(this.owners, 'stream read', (f) => nc.streamReadAll(this.ptr, f, buffer, length, readLength));
      const n = Number(koffi.decode(readLength, 'size_t'));
      return copyOut(buffer, n);
    } finally {
      koffi.free(buffer);
      koffi.free(readLength);
    }
  }

  async write(data: Uint8Array): Promise<void> {
    const buffer: Ptr = koffi.alloc('uint8_t', Math.max(data.length, 1));
    try {
      copyIn(buffer, data);
      await runChecked(this.owners, 'stream write', (f) => nc.streamWrite(this.ptr, f, buffer, data.length));
    } finally {
      koffi.free(buffer);
    }
  }

  /// Close our write direction. The device sees EOF.
  close(): Promise<void> {
    return runChecked(this.owners, 'stream close', (f) => nc.streamClose(this.ptr, f));
  }

  /// Abort outstanding reads/writes, wait for them to resolve and free the stream.
  async free(): Promise<void> {
    if (this.freed) {
      return;
    }
    this.freed = true;
    nc.streamAbort(this.ptr);
    await drainFutures(this.ptr);
    nc.streamFree(this.ptr);
  }
}

export interface MdnsResult {
  action: 'add' | 'update' | 'remove';
  productId: string;
  deviceId: string;
  serviceInstanceName: string;
  txtItems: Record<string, string>;
}

export class MdnsScanner {
  private listener: Ptr = 0n;
  private running = false;

  constructor(private readonly client: NabtoClient) {}

  /// Start scanning for Nabto devices on the local network. `onResult` is called
  /// for every add/update/remove until stop() is called.
  start(onResult: (r: MdnsResult) => void, onError?: (err: Error) => void): void {
    if (this.running) {
      return;
    }
    this.listener = nc.listenerNew(this.client.ptr);
    checkEc(nc.mdnsResolverInitListener(this.client.ptr, this.listener, null), 'init mdns listener');
    this.running = true;
    void this.loop(this.listener, onResult, onError);
  }

  stop(): void {
    if (!this.running) {
      return;
    }
    this.running = false;
    // The pending listener future resolves with STOPPED, the loop frees the listener.
    nc.listenerStop(this.listener);
  }

  private async loop(listener: Ptr, onResult: (r: MdnsResult) => void, onError?: (err: Error) => void): Promise<void> {
    const out: Ptr = koffi.alloc('void *', 1);
    try {
      for (;;) {
        const ec = await runFuture([this.client.ptr, listener], (f) => nc.listenerNewMdnsResult(listener, f, out));
        if (ec !== EC_OK) {
          if (errorString(ec) !== 'STOPPED') {
            onError?.(new NabtoError(ec, 'mdns'));
          }
          return;
        }
        const result = koffi.decode(out, 'void *') as Ptr;
        try {
          const action = (['add', 'update', 'remove'] as const)[nc.mdnsResultGetAction(result)] ?? 'update';
          let txtItems: Record<string, string> = {};
          try {
            txtItems = JSON.parse(nc.mdnsResultGetTxtItems(result) || '{}');
          } catch {
            // ignore malformed txt items
          }
          onResult({
            action,
            productId: nc.mdnsResultGetProductId(result) ?? '',
            deviceId: nc.mdnsResultGetDeviceId(result) ?? '',
            serviceInstanceName: nc.mdnsResultGetServiceInstanceName(result) ?? '',
            txtItems,
          });
        } finally {
          nc.mdnsResultFree(result);
        }
      }
    } finally {
      koffi.free(out);
      nc.listenerFree(listener);
    }
  }
}
