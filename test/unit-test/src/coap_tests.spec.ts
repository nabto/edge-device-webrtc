import { expect } from 'chai'
import { describe } from 'mocha'
import * as cbor from 'cbor'
import * as jwt from 'jsonwebtoken';
import { subtle } from 'crypto';
import { TestDevice, WebRTCNabtoClient, deviceInfo } from './fixture';


function str2ab(str: string): ArrayBuffer {
  var buf = new ArrayBuffer(str.length);
  var bufView = new Uint8Array(buf);
  for (var i=0, strLen=str.length; i<strLen; i++) {
    bufView[i] = str.charCodeAt(i);
  }
  return buf;
}


describe("coap endpoints", () => {
  let client: WebRTCNabtoClient;

  beforeEach(()=> {
    client = new WebRTCNabtoClient();
  });

  afterEach(() => {
    client.stop();
  });

  it("get signaling info", async ()=> {
    const conn = client.createConnection(deviceInfo.productId,deviceInfo.deviceId);
    await conn.connect();

    const req = conn.createCoapRequest("GET", "/webrtc/info");
    const resp = await req.execute();

    const code = resp.getResponseStatusCode();
    expect(code).to.equal(205);

    const cf = resp.getResponseContentFormat();
    expect(cf).to.equal(60);

    const payload = resp.getResponsePayload();
    // First cbor entry is the object containing the result
    const parsed = cbor.decodeFirstSync(payload);
    expect(parsed).to.have.property("FileStreamPort");
    expect(parsed.FileStreamPort).to.be.a('number');
    expect(parsed).to.have.property("SignalingStreamPort");
    expect(parsed.SignalingStreamPort).to.be.a('number');

    await conn.close();
  });

  it("fingerprint challenge", async ()=> {
    const conn = client.createConnection(deviceInfo.productId,deviceInfo.deviceId);
    await conn.connect();

    const req = conn.createCoapRequest("POST", "/webrtc/challenge");
    const reqObj = {challenge: "random-nonce"};

    const reqData = str2ab(JSON.stringify(reqObj));
    req.setRequestPayload(50, reqData);
    const resp = await req.execute();

    const code = resp.getResponseStatusCode();
    expect(code).to.equal(205);

    const cf = resp.getResponseContentFormat();
    expect(cf).to.equal(50);

    const payload = resp.getResponsePayload();
    // First cbor entry is the object containing the result
    const parsed = JSON.parse(String.fromCharCode.apply(null, new Uint8Array(payload)));
    expect(parsed).to.have.property("response");
    expect(parsed.response).to.be.a('string');

    const decodedJwt = jwt.decode(parsed.response, {complete: true});
    const signingData = parsed.response.substr(0, parsed.response.lastIndexOf('.'));

    const pubKey = await subtle.importKey(
      "jwk",
      decodedJwt.header.jwk,
      {
        name: "ECDSA",
        namedCurve: "P-256"
      },
      true,
      ["verify"]
    );

    const valid = await subtle.verify(
      {
        name: "ECDSA",
        hash: { name: "SHA-256"}
      },
      pubKey,
      Buffer.from(decodedJwt.signature, 'base64'),
      Buffer.from(signingData)
    );

    expect(valid).to.be.a('boolean');
    expect(valid).to.be.true;

    const pubKeyData = await subtle.exportKey("spki", pubKey);
    const fpBuf = await subtle.digest("SHA-256", pubKeyData);
    const fpArr = Array.from(new Uint8Array(fpBuf));
    const fp = fpArr
    .map((b) => b.toString(16).padStart(2, "0"))
    .join("");

    expect(fp).to.equal(deviceInfo.fingerprint);

    await conn.close();
  });



});
