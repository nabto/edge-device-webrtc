# Example of sending audio from the browser to the device.

## Start the RTP streams

Start the audio RTP sink.

```
gst-launch-1.0 -v udpsrc uri=udp://127.0.0.1:6003 caps="application/x-rtp,media=(string)audio,clock-rate=(int)48000,encoding-name=(string)X-GST-OPUS-DRAFT-SPITTKA-00" ! rtpopusdepay ! opusdec ! audioconvert ! autoaudiosink sync=false
```

## Start the device

./examples/simple-webrtc-send-audio-from-browser/simple_webrtc_device_send_audio_from_browser -p ... -d ... -k ...

## Client

This test app was used with a browser client built on the retired Nabto cloud signaling service, and that client has been removed. There is currently no client for it: the [desktop client](../../examples/desktop-client) only requests tracks through `POST /webrtc/tracks` and has no support for sending microphone audio to the device.
