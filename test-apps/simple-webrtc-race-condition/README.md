# Example of creating a race condition before perfect negotiation

## Start the RTP streams

Start the video sink such that video from the browser can be displayed. The sink is on port 6003

```
gst-launch-1.0 udpsrc uri=udp://127.0.0.1:6003 ! application/x-rtp, payload=96  ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! xvimagesink
```

Start a video feed which the browser shows.

```
gst-launch-1.0 videotestsrc ! clockoverlay ! video/x-raw,width=640,height=480 ! videoconvert ! queue ! x264enc tune=zerolatency bitrate=1000 key-int-max=30 ! video/x-h264, profile=constrained-baseline ! rtph264pay pt=96 mtu=1200 ! udpsink host=127.0.0.1 port=6000
```

## Start the device

./examples/simple-webrtc-race-condition/simple_webrtc_race_condition -p ... -d ... -k ...

## Open the webpage

First build the page. `cd web-client && npm install`

`file:///<path to index.html>`
