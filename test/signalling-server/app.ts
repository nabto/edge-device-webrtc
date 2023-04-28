

import { NabtoClientFactory } from 'edge-client-node'
import { WebSocketServer } from './websocket_listener';


const productId = "pr-4nmagfvj";
const deviceId = "de-sw9unmnn";
const serverUrl = "pr-4nmagfvj.devices.dev.nabto.net";


function stringToStreamObject(data: string): ArrayBuffer {
  let buf = new ArrayBuffer(data.length+4);
  let bufView = new Uint8Array(buf);
  bufView[0] = data.length & 0xFF;
  bufView[1] = (data.length & 0xFF00) >> 8;
  bufView[2] = (data.length & 0xFF0000) >> 16;
  bufView[3] = (data.length & 0xFF000000) >> 24;
  for (let i = 0; i  < data.length; i++) {
    bufView[i+4] = data.charCodeAt(i);
  }
  return buf;
}

function streamObjectToString(buf: ArrayBuffer): string {
  return Buffer.from(buf).toString('utf8');
}

async function main()
{
  let cli = NabtoClientFactory.create();
  cli.setLogLevel("info");
  cli.setLogCallback((logMessage) => {
      console.log(logMessage.message);
  });
  let key = cli.createPrivateKey();
  let conn = cli.createConnection();
  conn.setOptions({ProductId: productId, DeviceId: deviceId, PrivateKey: key, ServerUrl: serverUrl});
  await conn.connect();

  let stream = conn.createStream();
  await stream.open(42);

  let offer = {
    type: 0,
    data: "FOOBAR OFFER",
  }

  let offObj = stringToStreamObject(JSON.stringify(offer));
  console.log("writing offer object size:", JSON.stringify(offer).length, " buffer size: ", offObj.byteLength);
  await stream.write(offObj);

  let answer = {
    type: 1,
    data: "FOOBAR ANSWER",
  }

  let ansObj = stringToStreamObject(JSON.stringify(answer));
  console.log("writing answer", ansObj.byteLength);
  await stream.write(ansObj);

  let ice = {
    type: 2,
    data: "FOOBAR ICE",
  }

  let iceObj = stringToStreamObject(JSON.stringify(ice));
  console.log("writing ice", iceObj.byteLength);
  await stream.write(iceObj);

  let turn = {
    type: 3,
    data: "FOOBAR TURN REQUEST",
  }

  let turnObj = stringToStreamObject(JSON.stringify(turn));
  console.log("writing turn", turnObj.byteLength);
  await stream.write(turnObj);

  await stream.close();
  await conn.close();
  await cli.stop();

}

async function main2() {
  let serve = new WebSocketServer();
}

try {
  //main();
  main2();
} catch (e) {
  console.log("FAILURE: ", e);
}
