# edge-device-webrtc
WebRTC Implementation for Nabto Embedded SDK



## Example usage

In some terminal with gstreamer installed create an RTP stream using:
```
$ gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,width=640,height=480 ! videoconvert ! queue ! x264enc tune=zerolatency bitrate=1000 key-int-max=30 ! video/x-h264, profile=constrained-baseline ! rtph264pay pt=108 mtu=1200 ! udpsink host=127.0.0.1 port=6000
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
python3 -m http.server -d .
```

From a browser, go to `localhost:8000` press `log in`.

## TODO ideas

 * make docker container where libdatachannel and its dependencies are installed on the system and remove it as a git submodule here
 * use ephemeral stream port so we do not limit which ports costumers can use for their own streams
