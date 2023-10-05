
let spake = require("./spake2");
let cbor = require('cbor');
let jwt = require('jsonwebtoken');

class NabtoWebrtcConnection {

  myPeerConnection;
  coapDataChannel;

  coapRequests = new Map();

  uuidv4() {
    return ([1e7]+-1e3+-4e3+-8e3+-1e11).replace(/[018]/g, c =>
      (c ^ crypto.getRandomValues(new Uint8Array(1))[0] & 15 >> c / 4).toString(16)
    );
  }

  setPeerConnection(conn) {
    this.myPeerConnection = conn;
  }

  setCoapDataChannel(channel) {
    this.coapDataChannel = channel;
    let self = this;
    channel.addEventListener("message", (event) => {
      console.log("Got datachannel message: ", event.data);
      let data = JSON.parse(event.data);
      let cb = self.coapRequests.get(data.requestId);
      self.coapRequests.delete(data.requestId);
      cb(event.data);
    });
  }

  // TODO: switch to promises
  coapInvoke(method, path, contentType, payload, cb){
    if (!this.coapDataChannel) {
      throw new Error("CoAP data channel not configured");
    }

    // crypto.randomUUID() is not available on remote http
    let requestId = this.uuidv4();//crypto.randomUUID();

    let pl = payload;
    if (payload && typeof payload == typeof "string") {
      pl = Buffer.from(payload, "utf-8");
    }

    let req = {
      type: 0,
      requestId: requestId,
      method: method,
      path: path,
      contentType: contentType,
      payload: pl
    }

    this.coapRequests.set(requestId, cb);
    this.coapDataChannel.send(JSON.stringify(req));

  }

  decodeCborPayload(payload)
  {
    let val = cbor.decodeAllSync(Buffer.from(payload));
    return val[0];
  }

  // TODO: switch to promises
  passwordAuthenticate(username, password, callback) {
    if (!this.coapDataChannel) {
      throw new Error("CoAP data channel not configured");
    }

    let s = new spake(username, password);

    let T = s.calculateT();
    let obj = {
      T: T,
      Username: username
    }
    let payload = cbor.encode(obj);

    this.coapInvoke("POST", "/p2p/pwd-auth/1", 60, payload, (resp, error) => {
      if (error) {
        console.log("Coap invoke failure: ", error);
        callback(false);
        return;
      }
      console.log("Password round 1 response: ", resp);
      let response = JSON.parse(resp);

      let clifp = this.fpFromSdp(this.myPeerConnection.localDescription.sdp);
      let devFp = this.fpFromSdp(this.myPeerConnection.remoteDescription.sdp);

      s.calculateK(response.payload);
      let KcA = s.calculateKey(clifp, devFp);
      this.coapInvoke("POST", "/p2p/pwd-auth/2", 42, KcA, (resp2) => {
        console.log("Password round 2 resp: ", resp2);
        let response2 = JSON.parse(resp2);
        if (s.validateKey(response2.payload)) {
          callback(true);
        } else {
          callback(false);
        }

      });
    } );

  }

  readStream(channel, cb)
  {
    channel.addEventListener("message", (event) => {
      console.log("Got stream channel message: ", event.data);
      cb(event.data);
    });
  }


  fpFromSdp(sdp)
  {
    const searchStr = "a=fingerprint:sha-256 ";
    let fpAttStart = sdp.search(searchStr);
    if (fpAttStart == -1) {
      console.log("Failed to find fingerprint in SDP: ", sdp);
      return "";
    }
    let fp = sdp.substr(fpAttStart+searchStr.length, 64+31); //fp is 64 chars with a `:` between every 2 chars
    fp = fp.replace(/:/g, "");
    console.log("Found fingerprint: ", fp);
    return fp;
  }

  async validateJwt(token, fingerprint)
  {
    let decoded = jwt.decode(token, {complete: true});
    console.log("decoded JWT: ", decoded);
    console.log("importing JWK: ", decoded.header.jwk)

    let signingData = token.substr(0, token.lastIndexOf('.'));
    console.log("SigningData: ", signingData);

    let pubKey = await crypto.subtle.importKey(
      "jwk",
      decoded.header.jwk,
      {
        name: "ECDSA",
        namedCurve: "P-256"
      },
      true,
      ["verify"]);

    console.log("Signature: ", decoded.signature);
    console.log("Decoded signature: ", Buffer.from(decoded.signature, 'base64'));

    let valid = await crypto.subtle.verify(
      {
        name: "ECDSA",
        hash: { name: "SHA-256"}
      },
      pubKey,
      Buffer.from(decoded.signature, 'base64'),
      Buffer.from(signingData)
    );
    console.log("Token validity: ", valid);
    if (!valid) {
      return false;
    }

    let pubKeyData = await crypto.subtle.exportKey("spki",pubKey);
    let fpBuf = await crypto.subtle.digest("SHA-256", pubKeyData);
    let fpArr = Array.from(new Uint8Array(fpBuf));
    const fp = fpArr
    .map((b) => b.toString(16).padStart(2, "0"))
    .join("");
    console.log("FP: ", fp);

    return fp == fingerprint;

  }


};

module.exports = NabtoWebrtcConnection;
