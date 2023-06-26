

class NabtoWebrtcConnection {

  coapDataChannel;

  coapRequests = new Map();

  setCoapDataChannel(channel) {
    this.coapDataChannel = channel;
    let self = this;
    channel.addEventListener("message", (event) => {
      console.log("Got datachannel message: ", event.data);
      let data = JSON.parse(event.data);
      let cb = self.coapRequests.get(data.requestId);
      console.log("got callback", cb, " from requestId: ", data.requestId);
      console.log("looking for cb in: ", self.coapRequests);
      self.coapRequests.delete(data.requestId);
      cb(event.data);
    });
  }

  coapInvoke(method, path, contentType, payload, cb){
    if (!this.coapDataChannel) {
      throw new Error("CoAP data channel not configured");
    }


    let requestId = crypto.randomUUID();

    let req = {
      type: 0,
      requestId: requestId,
      method: method,
      path: path,
      contentType: contentType,
      payload: payload
    }

    console.log("invoking coap: ", req, " with CB: ", cb);

    this.coapRequests.set(requestId, cb);
    this.coapDataChannel.send(JSON.stringify(req));

  }


};
