// Node-only end-to-end check against a running edge_device_webrtc, without
// Electron or a browser WebRTC stack:
//
//   connect -> IAM pairing -> GET /iam/me -> signaling stream -> TURN request
//   -> POST /webrtc/tracks -> expect an SDP offer from the device.
//
// Usage:
//   npm run test:device -- '<pairing string>' [--key-file <path>] [--tracks a,b]
//   npm run test:device -- --mdns         # list devices on the LAN for 5 seconds

import fs from 'fs';
import { NabtoClient } from '../main/nabto/client';
import { SignalingChannel } from '../main/signaling';
import { getMe, getPairingInfo, pair } from '../main/iam';
import { parsePairingString } from '../shared/pairing-string';
import { SignalMessage, SignalMessageType } from '../shared/types';

function arg(name: string): string | undefined {
  const i = process.argv.indexOf(name);
  return i >= 0 ? process.argv[i + 1] : undefined;
}

function step(msg: string): void {
  console.log(`--> ${msg}`);
}

async function mdns(client: NabtoClient): Promise<void> {
  const scanner = client.createMdnsScanner();
  scanner.start((r) => console.log(`mdns ${r.action}: ${r.productId}.${r.deviceId} ${JSON.stringify(r.txtItems)}`),
    (err) => console.error(err.message));
  await new Promise((resolve) => setTimeout(resolve, 5000));
  scanner.stop();
}

async function run(client: NabtoClient): Promise<void> {
  const pairingString = process.argv.slice(2).find((a, i, all) => !a.startsWith('--') && !(all[i - 1] ?? '').startsWith('--'));
  if (!pairingString) {
    throw new Error("usage: device-smoke '<pairing string>' [--key-file path] [--tracks a,b] | --mdns");
  }
  const details = parsePairingString(pairingString);
  const tracks = (arg('--tracks') ?? 'frontdoor-video,frontdoor-audio').split(',');

  const keyFile = arg('--key-file');
  let key: string;
  if (keyFile && fs.existsSync(keyFile)) {
    key = fs.readFileSync(keyFile, 'utf8');
  } else {
    key = client.createPrivateKey();
    if (keyFile) {
      fs.writeFileSync(keyFile, key, { mode: 0o600 });
    }
  }

  step(`connect to ${details.productId}.${details.deviceId}`);
  const connection = client.createConnection();
  connection.setOptions({ ProductId: details.productId, DeviceId: details.deviceId, PrivateKey: key, ServerConnectToken: details.sct });
  await connection.connect();
  const deviceFp = connection.deviceFingerprint();
  console.log(`    connected (${connection.type()}), device fp ${deviceFp}, client fp ${connection.clientFingerprint()}`);
  if (details.deviceFingerprint && details.deviceFingerprint !== deviceFp) {
    throw new Error(`device fingerprint mismatch, expected ${details.deviceFingerprint}`);
  }

  step('GET /iam/pairing');
  const info = await getPairingInfo(connection);
  console.log(`    ${info ? `modes: [${info.modes.join(', ')}] app: ${info.appName} ${info.appVersion}` : 'device has no IAM'}`);
  if (info) {
    step('pair');
    const mode = await pair(connection, details, info);
    console.log(`    ${mode ? `paired using ${mode}` : 'already paired'}`);
    step('GET /iam/me');
    const me = await getMe(connection);
    console.log(`    ${JSON.stringify(me)}`);
  }

  step('open signaling stream');
  const signaling = await SignalingChannel.open(connection);
  const messages: SignalMessage[] = [];
  const waiters: Array<() => void> = [];
  const loop = signaling.readLoop((msg) => {
    messages.push(msg);
    waiters.splice(0).forEach((w) => w());
  });
  const waitFor = async (type: SignalMessageType, timeoutMs: number): Promise<SignalMessage> => {
    const deadline = Date.now() + timeoutMs;
    for (;;) {
      const found = messages.find((m) => m.type === type);
      if (found) {
        return found;
      }
      const remaining = deadline - Date.now();
      if (remaining <= 0) {
        throw new Error(`timed out waiting for signaling message type ${SignalMessageType[type]}`);
      }
      await Promise.race([new Promise<void>((r) => waiters.push(r)), new Promise((r) => setTimeout(r, remaining))]);
    }
  };

  step('TURN request');
  await signaling.send({ type: SignalMessageType.TurnRequest });
  const turn = await waitFor(SignalMessageType.TurnResponse, 10000);
  console.log(`    ${turn.iceServers?.length ?? 0} ice servers: ${(turn.iceServers ?? []).map((s) => s.urls.join(' ')).join(', ')}`);

  step(`POST /webrtc/tracks ${JSON.stringify(tracks)}`);
  const res = await connection.coap('POST', '/webrtc/tracks', { contentFormat: 50, payload: Buffer.from(JSON.stringify({ tracks })) });
  console.log(`    status ${res.status} ${res.payload.toString('utf8')}`);
  if (res.status !== 201) {
    throw new Error(`POST /webrtc/tracks failed with ${res.status}`);
  }

  step('wait for SDP offer from device');
  const offer = await waitFor(SignalMessageType.Offer, 10000);
  const sdp = JSON.parse(offer.data ?? '{}');
  const mlines = String(sdp.sdp).split('\r\n').filter((l: string) => l.startsWith('m=') || l.startsWith('a=rtpmap'));
  console.log(`    metadata ${JSON.stringify(offer.metadata)}`);
  mlines.forEach((l: string) => console.log(`    ${l}`));
  if (!mlines.some((l: string) => l.startsWith('m=video'))) {
    throw new Error('offer has no video section');
  }

  step('close');
  await signaling.close();
  console.log(`    read loop ended: ${await loop}`);
  await connection.close();
  console.log('OK');
}

async function main(): Promise<void> {
  console.log(`Nabto Client SDK ${NabtoClient.version()}`);
  const client = new NabtoClient();
  try {
    if (process.argv.includes('--mdns')) {
      await mdns(client);
    } else {
      await run(client);
    }
  } finally {
    await client.close();
  }
}

main().catch((err) => {
  console.error(`FAILED: ${err instanceof Error ? err.message : err}`);
  process.exit(1);
});
