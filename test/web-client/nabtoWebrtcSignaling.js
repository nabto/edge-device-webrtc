


class NabtoWebrtcSignaling {
  signalingHost = "ws://localhost:6503";
  productId;
  deviceId;
  sct;
  jwt;
  // TODO: serverkey;
  connection;

  /* Callback on websocket connected
   * (responseObject: {type: number}) => void
   */
  onconnected = null;

  /* Callback when offer is received
   * (offerObject: {type: number, data: string}) => void
   */
  onoffer = null;

  /* Callback when answer is received
   * (offerObject: {type: number, data: string}) => void
   */
  onanswer = null;

  /* Callback when ice candidate is received
   * (offerObject: {type: number, data: string}) => void
   */
  onicecandidate = null;

  /* Callback when turn credentials are received
   * (offerObject: {type: number, servers: {hostname: string, port: number, username: string, credential: string}[]}) => void
   */
  onturncredentials = null;

  /* Callback on errors
   * (errorString, errorObject) => void
   */
  onerror = null;


  setSignalingHost(host) {
    this.signalingHost = host;
  }

  setDeviceConfigSct(productId, deviceId, sct)
  {
    this.productId = productId;
    this.deviceId = deviceId;
    this.sct = sct;
  }

  setDeviceConfigJwt(productId, deviceId, jwt)
  {
    this.productId = productId;
    this.deviceId = deviceId;
    this.jwt = jwt;
  }

  signalingConnect() {
    if (!this.productId || !this.deviceId || !(this.sct || this.jwt)) {
      console.error("signallingConnect() called from invalid state. ProductId, deviceId, and either SCT or JWT must be configured first.");
      throw new Error("signallingConnect() called from invalid state. ProductId, deviceId, and either SCT or JWT must be configured first.");
    }
    let self = this;

    this.connection = new WebSocket(this.signalingHost, "json");

    this.connection.onerror = function (evt) {
      console.dir(evt);
      if (this.onerror) {
        this.onerror("Websocket connection error", evt);
      }
    }

    this.connection.onmessage = function (evt) {
      console.log("Message received: ", evt.data);
      var msg = JSON.parse(evt.data);

      switch (msg.type) {
        case 21: // WEBSOCK_LOGIN_RESPONSE
          if (self.onconnected) {
            self.onconnected(msg);
          }
          break;
        case 0:  // video offer (potentially used for renegotiation)
          if (self.onoffer) {
            self.onoffer(msg);
          }
          break;

        case 1:  // video answer from device
          if (self.onanswer) {
            self.onanswer(msg);
          }
          break;

        case 2: // A new ICE candidate has been received
          if (self.onicecandidate) {
            self.onicecandidate(msg);
          }
          break;

        case 4: // TURN response
          if (self.onturncredentials) {
            self.onturncredentials(msg);
          }
          break;

        default:
          console.error("Unknown message received:");
          console.error(msg);
          if (this.onerror) {
            this.onerror("Unknown Nabto Signaling message", msg);
          }

      }
    }
    this.connection.onopen = function (evt) {
      let loginReq = {
        type: 20,
        productId: self.productId,
        deviceId: self.deviceId,
        username: "foobar", // TODO: remove this signaling no longer makes IAM login
        password: "foobar", // TODO: remove this
      }
      if (self.sct) {
        loginReq.sct = self.sct;
      } else if (self.jwt) {
        loginReq.jwt = self.jwt;
      }

      self.sendToServer(loginReq);
    }
  }

  sendOffer(offer, metadata) {
    if (!this.connection) {
      throw new Error("Signalling connection not opened");
    }
    this.sendToServer({
      type: 0,
      data: JSON.stringify(offer),
      metadata: metadata
    });
  }

  sendAnswer(answer) {
    if (!this.connection) {
      throw new Error("Signalling connection not opened");
    }
    this.sendToServer({
      type: 1,
      data: JSON.stringify(answer)
    });
  }

  sendIceCandidate(candidate) {
    if (!this.connection) {
      throw new Error("Signalling connection not opened");
    }
    this.sendToServer({
      type: 2,
      data: JSON.stringify(candidate)
    });
  }


  requestTurnCredentials() {
    if (!this.connection) {
      throw new Error("Signalling connection not opened");
    }
    this.sendToServer({
      type: 3
    });

  }

  // INTERNAL FUNCTIONS
  sendToServer(msg) {
    var msgJSON = JSON.stringify(msg);
    console.log("Sending '" + msg.type + "' message: " + msgJSON);
    this.connection.send(msgJSON);
  }

};


module.exports = NabtoWebrtcSignaling;
