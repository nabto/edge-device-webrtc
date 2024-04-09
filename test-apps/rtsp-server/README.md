# RTSP test server

This is a standalone RTSP server implementation for testing. This is not part of the main project build by the top level CMakeLists.txt, but must be build separately.

## Prerequisites

This example requires GStreamer to be installed. On Debian, the dependencies are installed with these apt packages:

* `libgstrtspserver-1.0-dev`
* `libglib2.0-dev`
* `libgstreamer1.0-dev`
* `gstreamer1.0-plugins-ugly`

## Building

From this directory:

```
mkdir build
cd build
cmake ../
make -j
```

## Running

This example must be called with a GStreamer pipeline as argument. Additionally, the server can be configured with these options:
 * `-p`: The port to listen to (default: 8554)
 * `-e`: The RTSP endpoint (default: video)
 * `-a`: The Authentication mode to enforce. (options: `none`, `basic`, `digest`. Default: `none`)
 * `-u`: The username if using Basic or Digest auth (default: user)
 * `-P`: The password if using Basic or Digest auth (default: password)

Example starting H264 encoded video test source with Digest auth:

```
./src/gst-rtsp-launch '(  videotestsrc ! clockoverlay ! video/x-raw,width=640,height=480 ! videoconvert ! queue ! x264enc tune="zerolatency" byte-stream=true bitrate=1000 key-int-max=30 ! video/x-h264, profile=constrained-baseline ! rtph264pay name=pay0 pt=96 )' -a digest
```
