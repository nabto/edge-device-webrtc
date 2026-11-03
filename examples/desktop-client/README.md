# Nabto Edge WebRTC desktop client

A desktop demo client for Windows, Linux and macOS. It pairs with Nabto Edge WebRTC devices using decentralized IAM pairing and shows the video and audio feeds the devices send over WebRTC. Use it with the [`edge_device_webrtc`](../webrtc-demo) demo device or the [`simple-webrtc`](../simple-webrtc) example.

Features:

* Add a device by pasting the pairing string the device prints (`p=…,d=…,u=…,pwd=…,sct=…,fp=…`) or by picking it from the list of devices found on the local network (mDNS).
* IAM pairing modes `LocalInitial`, `LocalOpen`, `PasswordOpen` and `PasswordInvite`. The client uses the first mode the device offers that it has credentials for.
* Checks the device fingerprint from the pairing string before it pairs, so the client knows it is talking to the right device.
* Remembers paired devices, and keeps its private key encrypted with the OS keychain where available (Electron `safeStorage`).
* WebRTC video and audio playback with diagnostics: Nabto connection type, ICE path, codec, resolution, frame rate, bitrate, packet loss and per-track errors reported by the device.

## How it works

Browsers cannot open Nabto connections themselves, so the client is an [Electron](https://www.electronjs.org/) app:

```
Electron main process (Node.js)                        Renderer (Chromium)
  Nabto Edge Client SDK (C library, via koffi)           RTCPeerConnection, <video>/<audio>
  - Nabto connection, IAM pairing (CoAP + CBOR)  <-IPC->  pairing form, device list,
  - signaling stream to the device                        diagnostics
  - mDNS discovery
```

1. The main process connects to the device with the [Nabto Edge Client SDK](https://github.com/nabto/nabto-client-sdk-releases) and pairs through the IAM CoAP endpoints (`/iam/pairing/...`).
2. To start a WebRTC session it calls `GET /p2p/webrtc-info` and opens a Nabto stream to the returned signaling port. Signaling messages on the stream are a 4 byte little endian length followed by JSON.
3. The renderer asks the device for ICE servers, creates an `RTCPeerConnection` and requests tracks with `POST /webrtc/tracks`. The device sends an SDP offer with metadata that maps each media section to a track ID, and the client answers.

## Requirements

* Node.js 22 or newer and npm.
* On Linux, a desktop session with the usual Electron runtime libraries (GTK 3, NSS, ALSA).
* On Windows, the Microsoft Visual C++ runtime (`vcruntime140.dll`, `msvcp140.dll`), which the Nabto Client SDK needs. It is installed on most systems already.

## Running

```
cd examples/desktop-client
npm install
npm start
```

`npm install` also runs `scripts/fetch-nabto-sdk.js`, which downloads the prebuilt Nabto Client SDK library for your platform into `vendor/`. Set `NABTO_CLIENT_SDK_VERSION` to use another release than the pinned one, or `NABTO_CLIENT_LIBRARY` to point at a library file directly.

npm 11 asks before it runs install scripts of dependencies. `package.json` already allows the scripts the client needs (`koffi`, `electron-winstaller`) in its `allowScripts` section.

Then:

1. Start `edge_device_webrtc` (see the [top level README](../../README.md#running-the-example)). On first start it prints an initial pairing string.
2. Click **Add device**, paste the pairing string and click **Pair device**.
3. Select the device and click **Connect**.

Leave the *Tracks* field empty to request the device's default tracks (`GET /webrtc/tracks/default`, falling back to `frontdoor-video` and `frontdoor-audio`), or enter a comma separated list of track IDs.

Set `NABTO_LOG_LEVEL=trace` to get Nabto Client SDK logging on the console.

## Packaging

```
npm run dist
```

This builds a package for the current platform into `release/`: an AppImage on Linux, a dmg on macOS and an NSIS installer on Windows. Run `npm run dist -- --dir` to get an unpacked app instead. Packages are not code signed.

## Testing without the UI

`npm run test:device` runs the connect, pair and signaling flow from Node.js, without Electron. It is useful to check a device setup or the client logic:

```
npm run test:device -- 'p=pr-xxxx,d=de-xxxx,u=admin,pwd=...,sct=...,fp=...' [--key-file client-key.pem] [--tracks frontdoor-video]
npm run test:device -- --mdns    # list devices on the local network for 5 seconds
```

It connects, pairs, opens the signaling stream, requests tracks and checks that the device sends an SDP offer with a video section. Pass `--key-file` to keep the same client identity between runs.

`npm run test:unit` runs the unit tests.

## Troubleshooting

* **`NO_CHANNELS` when connecting:** the client could reach the device neither on the local network nor through the Nabto basestation. For remote connections the device must be attached to the basestation, and the pairing string must contain the server connect token (`sct`). On Linux, a firewall that blocks mDNS (UDP port 5353) prevents local connections.
* **LocalInitial/LocalOpen pairing is denied:** these modes only work over a local connection, so the client and the device must be on the same network.
* **The device denies the tracks request:** the client is connected but not paired, or its role has no `Webrtc:VideoStream` permission (the demo's `Unpaired` role can only do signaling).
* **No video but the connection is up:** check that the device has a video source (RTP on port 6000 by default, RTSP or a FIFO).

## Limitations

* The client does not send audio or video to the device.
* There is no user management (inviting, listing or removing users on a device).
