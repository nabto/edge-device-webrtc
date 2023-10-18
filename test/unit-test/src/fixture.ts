import { ChildProcess, execFile } from 'child_process';
import { NabtoClientFactory, NabtoClient, Connection, Stream } from 'edge-client-node'
import * as cbor from 'cbor'


export class WebRTCNabtoClient {
  mClient: NabtoClient;
  mPrivateKey: string = `-----BEGIN EC PRIVATE KEY-----
MHcCAQEEIINRFr3XQeYDlIsDOoWT4XpOKELSBIAuZE5jGBv7ASf8oAoGCCqGSM49
AwEHoUQDQgAEBlQ4BPJq5F4x2iKNBk9ELP3kbyfGSNULr7HgguVboUFbJAufVE9I
OsCaOqRjvcTGmefZlCR2k7TOwrM9//W8rQ==
-----END EC PRIVATE KEY-----
`;

  constructor() {
      this.mClient = NabtoClientFactory.create();
      // this.mPrivateKey = this.mClient.createPrivateKey(); // @TODO: Save and load private key from a file to persist it.
      // console.log(`Nabto SDK version: ${this.mClient.version()}`);
      this.mClient.setLogCallback(msg => console.log(`[NABTO::${msg.severity}]: ${msg.message}`));
  }

  async stop(): Promise<void> {
      this.mClient.stop();
  }

  createConnection(productId: string, deviceId: string) : Connection {
      const conn = this.mClient.createConnection();
      conn.setOptions({
          ProductId: productId,
          DeviceId: deviceId,
          ServerConnectToken: "demosct", // @TODO: Get a proper SCT from device
          PrivateKey: this.mPrivateKey,
          ServerUrl: `https://${productId}.clients.dev.nabto.net` // @TODO: Maybe have this domain template be set from Pulumi?
      });
      return conn;
  }

  async coapGet(conn: Connection, path: string, expectedStatus: number) {
    const req = conn.createCoapRequest("GET", path);
    const resp = await req.execute();

    const code = resp.getResponseStatusCode();
    if (code != expectedStatus) {
      throw new Error("Bad response code: " + expectedStatus + "!=" + code);
    }

    const cf = resp.getResponseContentFormat();
    const payload = resp.getResponsePayload();

    if (cf == 50) { // JSON
      const parsed = JSON.parse(String.fromCharCode.apply(null, new Uint8Array(payload)));
      return parsed;
    } else if (cf == 60) { // CBOR
      const parsed = cbor.decodeFirstSync(payload);
      return parsed;
    } else if (cf == 0) { // TEXT
      return String.fromCharCode.apply(null, new Uint8Array(payload));
    } else {
      throw new Error("Unknown Response Content Format: " + cf);
    }
  }

  async signalingStreamWriteObject(stream: Stream, input: any) {
    const inputStr = JSON.stringify(input);
    let buf = new ArrayBuffer(inputStr.length + 4);
    let bufView = new Uint8Array(buf);
    bufView[0] = inputStr.length & 0xFF;
    bufView[1] = (inputStr.length & 0xFF00) >> 8;
    bufView[2] = (inputStr.length & 0xFF0000) >> 16;
    bufView[3] = (inputStr.length & 0xFF000000) >> 24;
    for (let i = 0; i < inputStr.length; i++) {
      bufView[i + 4] = inputStr.charCodeAt(i);
    }
    return stream.write(buf);
  }
};

// TODO: do not use my default device

export const deviceInfo = {
  productId: "pr-4nmagfvj",
  deviceId: "de-sw9unmnn",
  fingerprint: "5fe0318b8e6e7b14fa30bba114ec0d98b6c5c3ba9f7aaa7babb639a03235c091",
  deviceKey: "b5b45deb271a63071924a219a42b0b67146e50f15e2147c9c5b28f7cf9d1015d",
  stsJwkUrl: "https://smarthome.tk.dev.nabto.com/sts/.well-known/jwks.json",
  stsIssuer: "myself",

};

export class TestDevice {
  device: ChildProcess;

  constructor() {

  }

  start(stdoutLog: boolean, nabtoLogLevel?: string) {
    let args = [
      '-d', deviceInfo.deviceId,
      '-p', deviceInfo.productId,
      '-k', deviceInfo.deviceKey,
      '--rtsp', 'rtsp://127.0.0.1:8554/video',
      '-j', deviceInfo.stsJwkUrl,
      '-i', deviceInfo.stsIssuer,
      '--iamreset',
    ];

    if (nabtoLogLevel) {
      args.push('--loglevel');
      args.push(nabtoLogLevel);
    }

    this.device = execFile('./edge_device_webrtc', args);
    if (stdoutLog) {
      this.device.stdout?.on('data', (data) => {
        console.log(data);
      });
    }
    this.device.on('error', (error) => {
      console.log("Exec error: ", error);
    });
    this.device.on('close', (code, signal) => {
      console.log("device closed with code ", code, " and signal: ", signal );
    });

  }

  stop() {
    this.device.kill('SIGINT');
  }
}
