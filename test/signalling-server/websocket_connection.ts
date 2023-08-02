import { CoapRequest, Connection, ConnectionOptions, NabtoClient, NabtoClientFactory, Stream } from 'edge-client-node';
import { connection, Message } from 'websocket'

const cbor = require('cbor');

export enum ObjectType {
  WEBRTC_OFFER = 0,
  WEBRTC_ANSWER = 1,
  WEBRTC_ICE = 2,
  TURN_REQUEST = 3,
  TURN_RESPONSE = 4,
  WEBSOCK_LOGIN_REQUEST = 20,
  WEBSOCK_LOGIN_RESPONSE = 21,
}

export class NabtoConnection {
  wsConn: WebSocketConnection;
  client: NabtoClient;
  conn: Connection;
  productId: string;
  deviceId: string;
  privateKey: string;
  serverUrl: string | undefined;
  stream: Stream | undefined;
  coap: CoapRequest | undefined;
  connected = false;

  async connectSct(sct: string): Promise<void> {
    let opts: ConnectionOptions = {
      ProductId: this.productId,
      DeviceId: this.deviceId,
      ServerConnectToken: sct,
      PrivateKey: this.privateKey,
      ServerUrl: this.serverUrl,
    }
    this.conn.setOptions(opts);
    await this.startConnection();
  }

  async startConnection() {
    // TODO: catch all promise rejections
    await this.conn.connect();

    this.coap = this.conn.createCoapRequest("GET", "/webrtc/info");
    let resp = await this.coap.execute();
    console.log("Got coap response");

    let port = 42;
    if (resp.getResponseStatusCode() == 205) {
      console.log("With status code 205");
      let respData = cbor.decodeAllSync(resp.getResponsePayload())[0]; // The cbor package returns the object as an array for some reason
      console.log("Parsed cbor payload as: ", respData);
      port = respData.SignalingStreamPort;
    } else {
      console.log("Coap response with status: ", resp.getResponseStatusCode());
    }

    console.log("Opening stream with streamPort: ", port);

    this.stream = this.conn.createStream();
    await this.stream.open(port);
    this.readStream();
    this.connected = true;
  }

  async readStream() {
    // TODO: catch all promise rejections
    try {
      while (true) {
        let objLenData = await this.stream?.readAll(4);
        if (!objLenData || objLenData.byteLength != 4) {
          // Stream failed
          // TODO: close the stream
          return;
        }
        let objLenView = new Uint32Array(objLenData);
        let objLen = objLenView[0];

        let objData = await this.stream?.readAll(objLen);
        if (!objData || objData.byteLength != objLen) {
          // Stream failed
          // TODO: close the stream
          return;
        }
        await this.wsConn.sendMessage(objData);
      }
    } catch (e) {
      console.log("readStream() exception: ", e);
      console.log("closing stream");
      await this.stream?.close();
      console.log("stream closed")
    }
  }

  async writeStreamString(data: string) {
    let buf = this.stringToStreamObject(data);
    console.log("Writing ", buf.byteLength, "bytes of data based on ", data.length, "bytes of content");
    while (true) {
    try {
    await this.stream?.write(buf);
    return;
    } catch(ex) {
      console.log("stream write failed with: ", ex);
    }
  }
  }

  async writeStreamBinary(data: ArrayBuffer) {
    let dataView = new Uint8Array(data);
    let buf = new ArrayBuffer(data.byteLength + 4);
    let bufView = new Uint8Array(buf);
    bufView[0] = data.byteLength & 0xFF;
    bufView[1] = (data.byteLength & 0xFF00) >> 8;
    bufView[2] = (data.byteLength & 0xFF0000) >> 16;
    bufView[3] = (data.byteLength & 0xFF000000) >> 24;
    for (let i = 0; i < data.byteLength; i++) {
      bufView[i + 4] = dataView[i];
    }
    await this.stream?.write(buf);
  }


  stringToStreamObject(data: string): ArrayBuffer {
    let buf = new ArrayBuffer(data.length + 4);
    let bufView = new Uint8Array(buf);
    bufView[0] = data.length & 0xFF;
    bufView[1] = (data.length & 0xFF00) >> 8;
    bufView[2] = (data.length & 0xFF0000) >> 16;
    bufView[3] = (data.length & 0xFF000000) >> 24;
    for (let i = 0; i < data.length; i++) {
      bufView[i + 4] = data.charCodeAt(i);
    }
    return buf;
  }
  setServerUrl(url: string): void {
    this.serverUrl = url;
  }

  isConnected(): boolean {
    return this.connected;
  }

  constructor(wsConn: WebSocketConnection, client: NabtoClient, privateKey: string, productId: string, deviceId: string) {
    this.wsConn = wsConn;
    this.client = client;
    this.privateKey = privateKey;
    this.productId = productId;
    this.deviceId = deviceId;
    this.conn = client.createConnection();
  }
}

export class WebSocketConnection {
  conn: connection;
  nabtoClient: NabtoClient;
  privateKey: string;
  nabtoConn: NabtoConnection | undefined;


  async handleMessage(message: Message): Promise<void> {
    // TODO: catch all promise rejections and close WS with proper error
    if (message.type === 'utf8') {
      console.log("Received Message: " + message.utf8Data);
      let msg = JSON.parse(message.utf8Data);

      if (msg.type == ObjectType.WEBSOCK_LOGIN_REQUEST) {
        if (!msg.productId ||
          !msg.deviceId ||
          !msg.username ||
          !msg.password ||
          !(msg.sct || msg.serverKey)) {
          console.log("Invalid request: ", msg);
          this.conn.close(1007, "SERVER_UNSUPPORTED_PAYLOAD");
          // TODO: handle cleanup in websocket server
          return;
        }
        this.nabtoConn = new NabtoConnection(this, this.nabtoClient, this.privateKey, msg.productId, msg.deviceId);
        if (msg.sct) {
          await this.nabtoConn.connectSct(msg.sct);
        } else {
          // TODO:
          console.log("ServerKey Connect is not implemented!!!!");
          return;
        }
        let resp = { type: ObjectType.WEBSOCK_LOGIN_RESPONSE };
        this.conn.send(JSON.stringify(resp));
        return;
      }

      if (this.nabtoConn && this.nabtoConn.isConnected()) {
        await this.nabtoConn.writeStreamString(message.utf8Data);
      }

    } else if (message.type === 'binary' && this.nabtoConn && this.nabtoConn.isConnected()) {
      await this.nabtoConn.writeStreamBinary(message.binaryData);
    }
    // TODO: else handle failure
  }

  async sendMessage(msg: ArrayBuffer) {
    this.conn.send(Buffer.from(msg).toString('utf8'));
  }

  constructor(conn: connection) {
    this.conn = conn;
    this.nabtoClient = NabtoClientFactory.create();
    this.privateKey = this.nabtoClient.createPrivateKey();
    let self = this;
    this.conn.on('message', function (message) {
      self.handleMessage(message);
      return;
    });
  }

}
