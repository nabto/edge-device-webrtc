// WebRTC signaling with an edge-device-webrtc device over a Nabto stream.
//
// Bootstrap: GET /p2p/webrtc-info returns {"SignalingStreamPort": n}, then a
// Nabto stream is opened to that port on the same connection. Each message on
// the stream is a 4 byte little endian length followed by that many bytes of
// UTF-8 JSON (see src/library/signaling-stream/signaling_stream.cpp).

import type { Connection, Stream } from './nabto/client';
import { NabtoError } from './nabto/client';
import type { SignalMessage } from '../shared/types';

const CF_JSON = 50;
const MAX_MESSAGE_SIZE = 1024 * 1024;

export class SignalingChannel {
  private writeQueue: Promise<void> = Promise.resolve();
  private closed = false;

  private constructor(private readonly stream: Stream) {}

  /// Open the signaling stream on `connection`.
  static async open(connection: Connection): Promise<SignalingChannel> {
    const res = await connection.coap('GET', '/p2p/webrtc-info');
    if (res.status !== 205) {
      throw new Error(res.status === 403
        ? 'The device denied access to WebRTC signaling (IAM action Webrtc:GetInfo).'
        : `GET /p2p/webrtc-info failed with status ${res.status}. Is this an edge-device-webrtc device?`);
    }
    if (res.contentFormat !== CF_JSON) {
      throw new Error(`GET /p2p/webrtc-info returned unexpected content format ${res.contentFormat}`);
    }
    const port = JSON.parse(res.payload.toString('utf8')).SignalingStreamPort;
    if (typeof port !== 'number') {
      throw new Error('GET /p2p/webrtc-info response has no SignalingStreamPort');
    }
    return new SignalingChannel(await connection.openStream(port));
  }

  /// Queue a message for sending. Writes are serialized so frames never interleave.
  send(msg: SignalMessage): Promise<void> {
    const json = Buffer.from(JSON.stringify(msg), 'utf8');
    const frame = Buffer.alloc(4 + json.length);
    frame.writeUInt32LE(json.length, 0);
    json.copy(frame, 4);
    const write = this.writeQueue.then(() => {
      if (this.closed) {
        throw new Error('Signaling channel is closed');
      }
      return this.stream.write(frame);
    });
    // Keep the queue alive after a failed write, the error is reported to the caller.
    this.writeQueue = write.catch(() => undefined);
    return write;
  }

  /// Read messages until the stream ends. Resolves with the reason the loop stopped.
  async readLoop(onMessage: (msg: SignalMessage) => void): Promise<string> {
    try {
      for (;;) {
        const header = await this.stream.readAll(4);
        const length = header.readUInt32LE(0);
        if (length === 0) {
          continue;
        }
        if (length > MAX_MESSAGE_SIZE) {
          return `Signaling message too large (${length} bytes)`;
        }
        const body = await this.stream.readAll(length);
        let msg: SignalMessage;
        try {
          msg = JSON.parse(body.toString('utf8'));
        } catch {
          continue; // the device also skips malformed messages
        }
        onMessage(msg);
      }
    } catch (err) {
      if (err instanceof NabtoError && (err.code === 'EOF' || err.code === 'STOPPED' || err.code === 'ABORTED')) {
        return this.closed ? 'closed by client' : 'signaling stream closed by the device';
      }
      return err instanceof Error ? err.message : String(err);
    }
  }

  /// Close the stream (the device tears down its PeerConnection on EOF) and free it.
  async close(): Promise<void> {
    if (this.closed) {
      return;
    }
    this.closed = true;
    try {
      await this.writeQueue;
      await this.stream.close();
    } catch {
      // ignore, we are shutting down anyway
    } finally {
      await this.stream.free();
    }
  }
}
