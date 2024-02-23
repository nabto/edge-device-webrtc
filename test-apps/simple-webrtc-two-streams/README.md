# Example of streaming more than one video feed.

## Start the RTP streams

```
gst-launch-1.0 videotestsrc ! clockoverlay ! textoverlay text="Feed 1" ! video/x-raw,width=640,height=480 ! videoconvert ! queue ! x264enc tune=zerolatency bitrate=1000 key-int-max=30 ! video/x-h264, profile=constrained-baseline ! rtph264pay pt=96 mtu=1200 ! udpsink host=127.0.0.1 port=6000
```

```
gst-launch-1.0 videotestsrc ! clockoverlay ! textoverlay text="Feed 2" ! video/x-raw,width=640,height=480 ! videoconvert ! queue ! x264enc tune=zerolatency bitrate=1000 key-int-max=30 ! video/x-h264, profile=constrained-baseline ! rtph264pay pt=96 mtu=1200 ! udpsink host=127.0.0.1 port=6001
```

## Start the device

./examples/simple-webrtc-two-streams/simple_webrtc_device_two_streams -p ... -d ... -k ...

## Open the webpage

First build the page. `cd web-client && npm install`

`file:///<path to index.html>`
