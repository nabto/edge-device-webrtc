#!/bin/bash

echo "Starting gst-rtsp-launch $@"

gst-rtsp-launch --gst-debug-level=3 '(  videotestsrc ! clockoverlay ! video/x-raw,width=640,height=480 ! videoconvert ! queue ! x264enc tune="zerolatency" byte-stream=true bitrate=1000 key-int-max=30 ! video/x-h264, profile=constrained-baseline ! rtph264pay name=pay0 pt=96 )' &

echo "running command $@"

/workspace/device/_build/examples/webrtc-demo/edge_device_webrtc -r rtsp://127.0.0.1:8554/video $@
