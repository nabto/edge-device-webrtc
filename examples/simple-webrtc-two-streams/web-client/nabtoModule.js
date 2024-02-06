const { decodeAllSync } = require('cbor');
var edgeWebrtc = require('edge-webrtc-client');

globalThis.window.EdgeWebrtc = edgeWebrtc;

class NabtoModuleUtil {
  decodeCborPayload(payload) {
    return decodeAllSync(Buffer.from(payload))[0];
  }
}

globalThis.window.nabtoModuleUtil = new NabtoModuleUtil;
