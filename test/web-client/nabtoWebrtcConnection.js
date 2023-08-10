
let spake = require("./spake2");
let cbor = require('cbor');

class NabtoWebrtcConnection {

  coapDataChannel;

  coapRequests = new Map();

  uuidv4() {
    return ([1e7]+-1e3+-4e3+-8e3+-1e11).replace(/[018]/g, c =>
      (c ^ crypto.getRandomValues(new Uint8Array(1))[0] & 15 >> c / 4).toString(16)
    );
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

    let req = {
      type: 0,
      requestId: requestId,
      method: method,
      path: path,
      contentType: contentType,
      payload: payload
    }

    this.coapRequests.set(requestId, cb);
    this.coapDataChannel.send(JSON.stringify(req));

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
      // TODO: use real fingerprints
      // let clifp = 'cff2f65cd103488b8cb2b93e838acc0f719d6deae37f8a4b74fa825244d28af8';
      // let devFp = '73e53042551c128a492cfd910b9ba67fffd2cab6c023b50c10992289f4c23d54';
      let clifp = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
      let devFp = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
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


};

module.exports = NabtoWebrtcConnection;
