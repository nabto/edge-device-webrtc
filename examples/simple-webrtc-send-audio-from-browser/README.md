# Example of sending audio from the browser to the device.

## Start the RTP streams

Start the audio RTP sink.

```
gst-launch-1.0 -v udpsrc uri=udp://127.0.0.1:6003 caps="application/x-rtp,media=(string)audio,clock-rate=(int)48000,encoding-name=(string)X-GST-OPUS-DRAFT-SPITTKA-00" ! rtpopusdepay ! opusdec ! audioconvert ! autoaudiosink sync=false
```

## Start the device

./examples/simple-webrtc-send-audio-from-browser/simple_webrtc_device_send_audio_from_browser -p ... -d ... -k ...

## Open the webpage

First build the page. `cd web-client && npm install`

`file:///<path to index.html>`
