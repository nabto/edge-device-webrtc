# edge-device-webrtc
WebRTC Implementation for Nabto Embedded SDK



## RTP usage

In some terminal with gstreamer installed create an RTP stream using:
```
$ gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,width=640,height=480 ! videoconvert ! queue ! x264enc tune=zerolatency bitrate=1000 key-int-max=30 ! video/x-h264, profile=constrained-baseline ! rtph264pay pt=96 mtu=1200 ! udpsink host=127.0.0.1 port=6000
```

From the repo root in a separate terminal:

```
mkdir build
cd build-scripts
docker compose up -d
docker compose exec device bash
cd build
cmake ../
make -j
./examples/webrtc-device/webrtc_device
```

in separate terminal:
```
cd test/signalling-server
npm install
ts-node app.ts
```

in separate terminal:
```
cd test/web-client
sudo npm install -g browserify
npm install
browserify nabtoWebrtc.js -o nabtoBundle.js
python3 -m http.server -d .
```

From a browser, go to `localhost:8000` press `log in`.

## RTSP usage

start the [rtsp-demo-container](https://github.com/nabto/rtsp-demo-container) with:

```
docker run --rm -it --network host rtsp-demo-server
```

Then start device with `--rtsp`:

```
./examples/webrtc-device/webrtc_device --rtsp rtsp://127.0.0.1:8554/video
```

### RTSP Details

This example this uses RTSP to start an RTP stream on port 45222. The UDP socket is bound to `0.0.0.0`, so it will also work with remote RTSP hosts.

The `rtsp_client.hpp` handling RTSP searches for `a = control: %32s` in the SDP document returned by the RTSP server to determine which URI to setup in the SETUP request. In the case of the openIPC cam we tested on, it would also send an audio channel which was picked at random.

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

