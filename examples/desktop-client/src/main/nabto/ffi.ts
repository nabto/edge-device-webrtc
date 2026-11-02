// Raw koffi bindings for the subset of the Nabto Edge Client SDK C API
// (nabto_client.h) used by this app. See client.ts for the Promise based
// wrappers that the rest of the app uses.
//
// koffi 3 represents all pointers as bigint. Handles (NabtoClient*,
// NabtoClientConnection*, ...) are therefore plain bigints here.

import fs from 'fs';
import path from 'path';
import koffi from 'koffi';

export type Ptr = bigint;

function libraryFileName(): string {
  switch (process.platform) {
    case 'win32': return 'nabto_client.dll';
    case 'darwin': return 'libnabto_client.dylib';
    default: return 'libnabto_client.so';
  }
}

function vendorDirName(): string {
  const platform = ({ darwin: 'mac', win32: 'win', linux: 'linux' } as Record<string, string>)[process.platform];
  return `${platform}-${process.arch}`;
}

/// Locate the native library. Order: $NABTO_CLIENT_LIBRARY, the packaged app's
/// resources dir, then vendor/<platform>-<arch> fetched by scripts/fetch-nabto-sdk.js.
function findLibrary(): string {
  const candidates: string[] = [];
  if (process.env.NABTO_CLIENT_LIBRARY) {
    candidates.push(process.env.NABTO_CLIENT_LIBRARY);
  }
  const resourcesPath = (process as NodeJS.Process & { resourcesPath?: string }).resourcesPath;
  if (resourcesPath) {
    candidates.push(path.join(resourcesPath, 'nabto', libraryFileName()));
  }
  // dist/main/nabto/ffi.js -> ../../../vendor
  candidates.push(path.join(__dirname, '..', '..', '..', 'vendor', vendorDirName(), libraryFileName()));
  if (process.platform === 'darwin') {
    candidates.push(path.join(__dirname, '..', '..', '..', 'vendor', 'mac-universal', libraryFileName()));
  }
  const found = candidates.find((c) => fs.existsSync(c));
  if (!found) {
    throw new Error(`Nabto Client SDK library not found. Looked in:\n  ${candidates.join('\n  ')}\n` +
      'Run "node scripts/fetch-nabto-sdk.js" or set NABTO_CLIENT_LIBRARY.');
  }
  return found;
}

const lib = koffi.load(findLibrary());

// NABTO_CLIENT_API is __stdcall on Windows. It only matters for 32 bit x86,
// koffi ignores it elsewhere.
const API = process.platform === 'win32' ? '__stdcall' : '';

export const FutureCallback = koffi.proto(`void ${API} NabtoClientFutureCallback(void *future, int ec, void *data)`);

function fn(decl: string) {
  return lib.func(decl.replace('API', API));
}

export const nc = {
  version: fn('const char * API nabto_client_version()'),
  errorGetMessage: fn('const char * API nabto_client_error_get_message(int ec)'),
  errorGetString: fn('const char * API nabto_client_error_get_string(int ec)'),
  stringFree: fn('void API nabto_client_string_free(void *str)'),

  clientNew: fn('void * API nabto_client_new()'),
  clientFree: fn('void API nabto_client_free(void *client)'),
  clientStop: fn('void API nabto_client_stop(void *client)'),
  setLogLevel: fn('int API nabto_client_set_log_level(void *client, const char *level)'),
  createPrivateKey: fn('int API nabto_client_create_private_key(void *client, _Out_ void **key)'),

  futureNew: fn('void * API nabto_client_future_new(void *client)'),
  futureFree: fn('void API nabto_client_future_free(void *future)'),
  futureSetCallback: fn('void API nabto_client_future_set_callback(void *future, NabtoClientFutureCallback *cb, void *data)'),

  connectionNew: fn('void * API nabto_client_connection_new(void *client)'),
  connectionFree: fn('void API nabto_client_connection_free(void *connection)'),
  connectionStop: fn('void API nabto_client_connection_stop(void *connection)'),
  connectionSetOptions: fn('int API nabto_client_connection_set_options(void *connection, const char *json)'),
  connectionGetOptions: fn('int API nabto_client_connection_get_options(void *connection, _Out_ void **json)'),
  connectionConnect: fn('void API nabto_client_connection_connect(void *connection, void *future)'),
  connectionClose: fn('void API nabto_client_connection_close(void *connection, void *future)'),
  connectionPasswordAuthenticate: fn('void API nabto_client_connection_password_authenticate(void *connection, const char *username, const char *password, void *future)'),
  connectionGetDeviceFingerprint: fn('int API nabto_client_connection_get_device_fingerprint(void *connection, _Out_ void **fp)'),
  connectionGetClientFingerprint: fn('int API nabto_client_connection_get_client_fingerprint(void *connection, _Out_ void **fp)'),
  connectionGetType: fn('int API nabto_client_connection_get_type(void *connection, _Out_ int *type)'),
  connectionGetLocalChannelErrorCode: fn('int API nabto_client_connection_get_local_channel_error_code(void *connection)'),
  connectionGetRemoteChannelErrorCode: fn('int API nabto_client_connection_get_remote_channel_error_code(void *connection)'),

  coapNew: fn('void * API nabto_client_coap_new(void *connection, const char *method, const char *path)'),
  coapFree: fn('void API nabto_client_coap_free(void *coap)'),
  coapSetRequestPayload: fn('int API nabto_client_coap_set_request_payload(void *coap, uint16_t cf, const void *payload, size_t len)'),
  coapExecute: fn('void API nabto_client_coap_execute(void *coap, void *future)'),
  coapGetResponseStatusCode: fn('int API nabto_client_coap_get_response_status_code(void *coap, _Out_ uint16_t *status)'),
  coapGetResponseContentFormat: fn('int API nabto_client_coap_get_response_content_format(void *coap, _Out_ uint16_t *cf)'),
  coapGetResponsePayload: fn('int API nabto_client_coap_get_response_payload(void *coap, _Out_ void **payload, _Out_ size_t *len)'),

  streamNew: fn('void * API nabto_client_stream_new(void *connection)'),
  streamFree: fn('void API nabto_client_stream_free(void *stream)'),
  streamAbort: fn('void API nabto_client_stream_abort(void *stream)'),
  streamOpen: fn('void API nabto_client_stream_open(void *stream, void *future, uint32_t port)'),
  streamReadAll: fn('void API nabto_client_stream_read_all(void *stream, void *future, void *buffer, size_t len, void *readLength)'),
  streamWrite: fn('void API nabto_client_stream_write(void *stream, void *future, void *buffer, size_t len)'),
  streamClose: fn('void API nabto_client_stream_close(void *stream, void *future)'),

  listenerNew: fn('void * API nabto_client_listener_new(void *client)'),
  listenerFree: fn('void API nabto_client_listener_free(void *listener)'),
  listenerStop: fn('void API nabto_client_listener_stop(void *listener)'),
  mdnsResolverInitListener: fn('int API nabto_client_mdns_resolver_init_listener(void *client, void *listener, const char *subtype)'),
  listenerNewMdnsResult: fn('void API nabto_client_listener_new_mdns_result(void *listener, void *future, void *result)'),
  mdnsResultFree: fn('void API nabto_client_mdns_result_free(void *result)'),
  mdnsResultGetDeviceId: fn('const char * API nabto_client_mdns_result_get_device_id(void *result)'),
  mdnsResultGetProductId: fn('const char * API nabto_client_mdns_result_get_product_id(void *result)'),
  mdnsResultGetServiceInstanceName: fn('const char * API nabto_client_mdns_result_get_service_instance_name(void *result)'),
  mdnsResultGetTxtItems: fn('const char * API nabto_client_mdns_result_get_txt_items(void *result)'),
  mdnsResultGetAction: fn('int API nabto_client_mdns_result_get_action(void *result)'),
};

/// NABTO_CLIENT_EC_OK is 0 in all SDK versions, other codes are looked up by
/// name through nabto_client_error_get_string() since they are exported as
/// variables rather than macros.
export const EC_OK = 0;

/// Short error name, e.g. "EOF" for NABTO_CLIENT_EC_EOF.
export function errorString(ec: number): string {
  return (nc.errorGetString(ec) as string).replace(/^NABTO_CLIENT_EC_/, '');
}

export function errorMessage(ec: number): string {
  return nc.errorGetMessage(ec) as string;
}

/// Read a char* allocated by the SDK and free it with nabto_client_string_free.
export function takeSdkString(ptr: Ptr): string {
  if (!ptr) {
    return '';
  }
  const s = koffi.decode(ptr, 'char', -1) as string;
  nc.stringFree(ptr);
  return s;
}

export { koffi };
