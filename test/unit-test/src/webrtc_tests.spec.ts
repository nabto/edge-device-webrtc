import { expect } from 'chai'
import { describe } from 'mocha'
import { WebRTCNabtoClient, deviceInfo } from './fixture';
import { RTCPeerConnection, RTCSessionDescription } from 'werift';

describe.only("webrtc", () => {
  let client: WebRTCNabtoClient;

  beforeEach(()=> {
    client = new WebRTCNabtoClient();
  });

  afterEach(() => {
    client.stop();
  });

  it("create signaling stream", async ()=> {
    const conn = client.createConnection(deviceInfo.productId,deviceInfo.deviceId);
    await conn.connect();

    const info = await client.coapGet(conn, "/webrtc/info", 205);

    expect(info).to.have.property("FileStreamPort");
    expect(info.FileStreamPort).to.be.a('number');
    expect(info).to.have.property("SignalingStreamPort");
    expect(info.SignalingStreamPort).to.be.a('number');

    const stream = conn.createStream();
    await stream.open(info.SignalingStreamPort);

    await stream.close();
    await conn.close();
  });

  it("Get turn servers", async ()=> {
    const conn = client.createConnection(deviceInfo.productId,deviceInfo.deviceId);
    await conn.connect();

    const info = await client.coapGet(conn, "/webrtc/info", 205);
    const stream = conn.createStream();
    await stream.open(info.SignalingStreamPort);

    const turnReq = {type: 3};
    await client.signalingStreamWriteObject(stream, turnReq);

    const turnResp = await stream.readSome();

    expect(turnResp.byteLength).to.be.greaterThan(4);
    let respView = new Uint8Array(turnResp);
    let objLen = respView[0] + (respView[1] << 8) + (respView[2] << 16) + (respView[3] << 24);

    expect(objLen).to.equal(turnResp.byteLength-4);

    const parsed = JSON.parse(String.fromCharCode.apply(null, new Uint8Array(turnResp).subarray(4)));

    expect(parsed).to.have.property("servers");
    expect(parsed).to.have.property("type");
    expect(parsed.servers).to.be.an("array");
    expect(parsed.type).to.be.a("number").and.to.equal(4);

    await stream.close();
    await conn.close();
  });

  it("Establish webrtc channel", async ()=> {
    const conn = client.createConnection(deviceInfo.productId,deviceInfo.deviceId);
    await conn.connect();

    const info = await client.coapGet(conn, "/webrtc/info", 205);
    const stream = conn.createStream();
    await stream.open(info.SignalingStreamPort);

    let resolver: (value: void | PromiseLike<void>) => void;
    const p = new Promise<void>((resolve, reject) => {
      resolver = resolve;
    })

    const peer = new RTCPeerConnection();
    // peer.onicecandidate = (event) => {
    //   console.log("ICE ICE BABY");
    // };

    const coapChan = peer.createDataChannel("coap");

    coapChan.onopen = () => {
      resolver();
    }


    const offer = await peer.createOffer();
    await peer.setLocalDescription(offer);

    const offerMsg = {
      type: 0,
      data: JSON.stringify(peer.localDescription),
      metadata: { noTrickle: true }
    };


    await client.signalingStreamWriteObject(stream, offerMsg);

    const streamDataLen = await stream.readAll(4);
    const streamData = await stream.readAll(client.streamDataToLen(streamDataLen));
    const parsedStreamData = client.signalingParseJson(streamData);

    let remoteDesc = JSON.parse(parsedStreamData.data);

    await peer.setRemoteDescription(remoteDesc);

    await p;

    await peer.close();

    await stream.close();
    await conn.close();
  });


});