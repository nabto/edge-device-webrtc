# edge-device-webrtc
WebRTC example implementation for Nabto Embedded SDK

## Current limitations

 * Nabto WebRTC uses signaling over Nabto Streaming meaning the signaling server is only needed by browsers (native Nabto Clients creates their own signaling stream directly). The example is currently ONLY tested with a browser through the signaling server (Though native clients should work fine).
 * IAM is configured with very poor security. (bad passwords etc.)
 * The demo uses the DEV basestation as TURN credentials are not deployed to prod.
 * `--rtpport` can be used to set the port number of the video video. It is then assumed the audio feed is on `port+1` and that the received audio should be sent to `port+2`;
 * H264 video codec (it is pretty simple to add codecs)
 * PCMU audio codec for RTSP feeds and OPUS audio codec for RTP feeds (Still simple to add/change codecs, but requires code changes to switch)
 * limited RTSP support:
 * - Only supports device sending video/audio feed, no recieving audio
 * - The device will receive RTCP packets, and return dummy Receiver reports as our test cam uses this as keep alive.
 * No RTCP for RTP streams. We have not found any streamers which actually reacts to RTCP, so we do not handle it.


## RTP usage

The RTP example supports sending video to the client, and sending and receiving audio from the client. This example handles these 3 feeds using gstreamer, but any RTP source/sink can be used as long as they use the proper UDP ports and codecs.

### Create a video feed
In some terminal with gstreamer installed create an RTP stream using:
```
$ gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,width=640,height=480 ! videoconvert ! queue ! x264enc tune=zerolatency bitrate=1000 key-int-max=30 ! video/x-h264, profile=constrained-baseline ! rtph264pay pt=96 mtu=1200 ! udpsink host=127.0.0.1 port=6000
```

### Create an audio feed (optional)
In some terminal with gstreamer installed create an RTP stream using:
```
gst-launch-1.0 -v audiotestsrc wave=sine freq=220 volume=0.01 ! audioconvert ! opusenc ! rtpopuspay name=pay0 pt=111 ! udpsink host=127.0.0.1 port=6001
```

### Create an audio sink (optional)
In some terminal with gstreamer installed create an RTP stream using:
```
gst-launch-1.0 -v udpsrc uri=udp://127.0.0.1:6002 caps="application/x-rtp,media=(string)audio,clock-rate=(int)48000,encoding-name=(string)X-GST-OPUS-DRAFT-SPITTKA-00" ! rtpopusdepay ! opusdec ! audioconvert ! autoaudiosink sync=false
```

### Running the device
From the repo root in a separate terminal:

```
mkdir build
cd build-scripts
docker compose up -d
docker compose exec device bash
cd build
cmake ../
make -j
cp ../src/nabto-device/nabto.png .
./src/edge_device_webrtc -d <DEVICE_ID> -p <PRODUCT_ID> -k <RAW_PRIVATE_KEY>
```

### Demo Signaling and browser client
For demo usage, use our deployed signaling and browser client by opening `http://34.245.62.208:8000/` in your browser.

set product ID, device ID, and SCT, and press `connect` to connect to the device. This will make a WebRTC connection to the device with a datachannel (labelled `coap`) only.

To get access to the video feed, you must authenticate with the device. To use Oauth authentication, contact the developer. For password authentication, set the `IAM Username` and `IAM Password` and press `Password Authenticate`. The username and password will default to the admin user created in the default IAM state of the device.

After authentication, press `Request video` to show the video feed. This will
 * Use the datachannel to invoke CoAP `GET /webrtc/video/frontdoor-video`,
 * causing the device to add the video feed to the peer connection and initiating renegotiation.
 * The video feed is shown.

If you created the optional audio feeds, press `Add Audio` which will:
 * Use the datachannel to invoke CoAP `GET /webrtc/video/frontdoor-audio`,
 * causing the device to add the audio feed to the peer connection and initiating renegotiation.
 * While setting up the audio feed, the browser will create an audio feed from your default mic and add it to the peer connection, again, requiring renegotitaion.
 * The browser will play the audio feed from the device and the device will stream the audio feed from the browser to UDP.

Press the `Get Image` button. This will:
 * Create a new data channel labelled `stream-655`
 * causing the device to create a virtual Nabto stream to stream port `655`
 * When the stream is opened, the device will stream the `nabto.png` picture to the browser and close the stream.
 * When the stream is closed, the browser will render the image.

 Checking the `offer to receive` checkbox before pressing `log in` will add the datachannel, and the video and audio transceivers to the peer connection in the browser before making the first offer. This is used to test how an Alexa feed will work.


### Development Signaling and browser client
in separate terminal:
```
cd test/web-client
sudo npm install -g browserify
npm install
browserify nabtoWebrtc.js -o nabtoBundle.js
python3 -m http.server -d .
```

From a browser, go to `localhost:8000` press `log in`.

This browser client will use the deployed demo signaling server. To use a local signaling server, go to file `test/web-client/videoClient.js` and remove the line:
```
  nabtoSignaling.setSignalingHost("ws://34.245.62.208:6503");
```

Refresh the webpage to apply the change.

in separate terminal:
```
cd test/signalling-server
npm install
ts-node app.ts
```

## RTSP usage

This example uses a Nabto provided RTSP demo container. However, any RTSP server can be used. It is important that the RTSP server and the device example has free network access to each other to enable the UDP streaming sockets to work properly. This is why the example uses `--network host`.

start the [rtsp-demo-container](https://github.com/nabto/rtsp-demo-container) with:

```
docker run --rm -it --network host rtsp-demo-server
```

Then start device with `--rtsp`:

```
./examples/webrtc-device/webrtc_device --rtsp rtsp://127.0.0.1:8554/video
```

The streaming can now be tested with the web client in the same way as the RTP example above. **However, the RTSP demo container does not provide an audio feed so add Audio will not work.** Providing an RTSP server with an audio feed will make one-way audio work.

### RTSP Details

This example this uses RTSP to start an RTP video stream on port 45222 and, if the RTSP server has one, an RTP audio stream on port 45224. The UDP socket is bound to `0.0.0.0`, so it will also work with remote RTSP hosts.

The RTSP client only supports one video and one audio stream. If the servers response to the RTSP `Describe` request contains more than one stream of a kind (video/audio), one will be picked and the remaining will be ignored.


## Usage details

For ease of development usage, the device starts with default Nabto device configuration for a dev device ID. This feature will be removed soon. For general usage, these device options should not be optional:

- `--serverurl` BS serverurl currently defaults to dev bs.
- `--productid`
- `--deviceid`
- `--privatekey` the private key used by the device in `RAW` format. The fingerprint must be registered in the console.

These options will remain optional:
- `--sct` Defaults to `demosct`
- `--rtsp` If set to an RTSP url, the device will get the RTP stream using RTSP.
- `--rtpport` If `--rtsp` is NOT used, the device binds to UDP on this port to receive RTP data. Defaults to 6000.

## Implementation details

The implementation follows this architecture:

![arch](code-architecture.png)

In this example, the browser will create a WebRTC connection by sending an offer only requesting a data channel. This data channel is then used to invoke a CoAP endpoint causing the device to add the desired media tracks to the WebRTC connection and then initiate a renegotiation by sending a new offer to the browser. This is done because 1) The data channel can be used for authentication before being allowed to get the media tracks and 2) The browser (at least chrome) will not support RTCP packets for a given ssrc unless it was in the negotiation from the start (ie. the track is in the offer).
