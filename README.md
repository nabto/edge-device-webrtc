# Edge Device Webrtc

This WebRTC example implementation for Nabto Embedded SDK allows clients to establish WebRTC connections to the device using Nabto. This WebRTC connection can then be used to stream one-way video and two-way audio feeds from either RTP or RTSP. WebRTC data channels can also be used to invoke CoAP endpoint in the device as well as streaming data through Nabto Streaming.

In addition to WebRTC, this example also implements OAuth to authenticate connections in the IAM module.

Note that Firefox' WebRTC support is limited and is not currently supported by the example application.

Note that the example applications use the RTP client in `./src/modules/rtp-client`. This is used to relay RTP packets between a UDP socket and WebRTC. This implementation is based on Unix sockets, and will not work on Windows.

## Obtaining the Source

This Github repo references various 3rd party components as submodules. So remember to clone recursively:

```
git clone --recursive https://github.com/nabto/edge-device-webrtc.git
```

## Demo container
For demo purposes, this example can be run in a Docker container. This requires Docker to be installed on your system, but ensures all dependencies are installed and working.
After cloning this repo as shown above, the demo can be build using:

```
docker build . -t edge-device-webrtc
```

Before starting the example, you need a device configured in the Nabto Cloud Console. This requires you to configure a fingerprint. To obtain this, run the demo with the `--create-key` argument:

```
docker run -it --rm edge-device-webrtc edge_device_webrtc --create-key
Created Raw private key:
  badfae3acfa7ab904ac639f0c4cb0ad268d23f4e324e0708aeb567f87da0c324
With fingerprint:
  73e53042551c128a492cfd910b9ba67fffd2cab6c023b50c10992289f4c23d54
```
The *Raw key* must be used when starting the example, the *Fingerprint* should be configured in the Nabto Cloud Console.

If you do not already have a device in the Nabto Cloud Console, follow our [general guide for embedded applications](https://docs.nabto.com/developer/platforms/embedded/applications.html#console).

After configuring the device fingerprint, the example device will be able to attach to the Nabto Basestation when starting the demo. The demo application persists its list of users to a local file. For this file to persist between container restarts, create a directory to mount as a docker volume before running the demo:

```
mkdir webrtc-home
docker run -v `pwd`/webrtc-home:/homedir -it --rm edge-device-webrtc edge_device_webrtc -r rtsp://127.0.0.1:8554/video -H /homedir -d <YOUR_DEVICE_ID> -p <YOUR_PRODUCT_ID> -k <RAW_KEY_CREATED_ABOVE>
```

## Tools
You need the following tools to build the example:

* Git
* [CMake](https://cmake.org/)
* C++ compiler
* curl zip unzip tar

## Building
The example is built using cmake from the root of this repo:

```
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=`pwd`/install ..
make -j16 install
```

On macOS you may need to turn off `sctp_werror` so the full set of commands become:
```
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=`pwd`/install -Dsctp_werror=OFF ..
make -j16 install
```

If your build host has limited resources and you try to limit the build concurrency using e.g. `make -j1`, you also need to explicitly limit the vcpkg build concurrency using `export VCPKG_MAX_CONCURRENCY=1` prior to running cmake.

## Running

To start the device you must first either have RTP feeds or an RTSP server started. If you do not already have these, see [the sections below](#gstreamer-rtp-feeds) on how to start these.

Before starting the example, you need a device configured in the Nabto Cloud Console. This requires you to configure a fingerprint. To obtain this, run the example application with the `--create-key` argument:

```
./edge_device_webrtc --create-key
Created Raw private key:
  badfae3acfa7ab904ac639f0c4cb0ad268d23f4e324e0708aeb567f87da0c324
With fingerprint:
  73e53042551c128a492cfd910b9ba67fffd2cab6c023b50c10992289f4c23d54
```

The *Raw key* must be used when starting the example, the *Fingerprint* should be configured in the Nabto Cloud Console.

If you do not already have a device in the Nabto Cloud Console, follow our [general guide for embedded applications](https://docs.nabto.com/developer/platforms/embedded/applications.html#console).

After configuring the device fingerprint, the example device will be able to attach to the Nabto Basestation when started.
### Running with RTP
Assuming your RTP feeds are running on the default ports, the device is started with:

```
./edge_device_webrtc -d <YOUR_DEVICE_ID> -p <YOUR_PRODUCT_ID> -k <RAW_KEY_CREATED_ABOVE>
################################################################
# Initial user pairing link:
# https://demo.smartcloud.nabto.com?p=pr-f4nqpowq&d=de-jseuziej&u=admin&pwd=tyMYx9dgAygC&sct=m8ntwSkKkbcf&fp=73e53042551c128a492cfd910b9ba67fffd2cab6c023b50c10992289f4c23d54
################################################################
Device: pr-f4nqpowq.de-jseuziej with fingerprint: [73e53042551c128a492cfd910b9ba67fffd2cab6c023b50c10992289f4c23d54]
2023-11-03 07:32:30.620 ...sdk/src/core/nc_device.c(399)[_INFO] Starting Nabto Device
medias size: 1
2023-11-03 07:32:30.906 ...k/src/core/nc_attacher.c(747)[_INFO] Device attached to basestation
```

### Running with RTSP
To use an RTSP server the `--rtsp` argument is used to set the RTSP URL to use. For the RTSP server shown in the RTSP feeds section, the device is started with:
```
./edge_device_webrtc -d <YOUR_DEVICE_ID> -p <YOUR_PRODUCT_ID> -k <RAW_KEY_CREATED_ABOVE> --rtsp rtsp://127.0.0.1:8554/video
################################################################
# Initial user pairing link:
# https://demo.smartcloud.nabto.com?p=pr-f4nqpowq&d=de-jseuziej&u=admin&pwd=tyMYx9dgAygC&sct=m8ntwSkKkbcf&fp=73e53042551c128a492cfd910b9ba67fffd2cab6c023b50c10992289f4c23d54
################################################################
Device: pr-f4nqpowq.de-jseuziej with fingerprint: [73e53042551c128a492cfd910b9ba67fffd2cab6c023b50c10992289f4c23d54]
2023-11-03 07:32:30.620 ...sdk/src/core/nc_device.c(399)[_INFO] Starting Nabto Device
medias size: 1
2023-11-03 07:32:30.906 ...k/src/core/nc_attacher.c(747)[_INFO] Device attached to basestation
```

### Connecting to the device
To connect to your device, follow the `Initial user pairing link` printed by the device when first started. This will open our WebRTC demo website and initiate pairing with the device.

First you must create an account in the demo backend. Click `Sign in`, then click `Sign up`. Create your account using an email and a password. Once your account is verifed, you will be redirected back to the device pairing flow.

Now, accept the device invitation. This will add the device information to your account in the backend. It then connects to the device, verify the device fingerprint, perform password authentication as the initial user, configure the OAuth subject of your account on the initial user, and finally removes the password of the initial user.

When the invitation is accepted, you can connect to the device and authenticate using OAuth and see the video feed. When connected to the device, you can also invite other users to your device. This will create a new IAM user on the device, and generate a link similar to the `initial user pairing link` allowing another user to go through the same invite flow as described here.

### Other connect options
The device can also be accessed through Android/iOS Apps, Google Home, and Alexa. These functionalities are not yet finalized. For more information about these features, contact Nabto support.

## GStreamer RTP feeds

The RTP example supports sending video to the client, and sending and receiving audio from the client. This section shows how to start these 3 feeds using gstreamer, but any RTP source/sink can be used as long as they use the proper UDP ports and codecs.

By default, the example expects an RTP video feed on UDP port 6000, an RTP video sink on UDP port 6001, an RTP audio feed on UDP port 6002, and an RTP audio sink on UDP port 6004. These ports can be changed using the `--rtp-port arg` argument. The port configured by this argument will be the first port (defaulting to 6000). The 4 port numbers will always be consecutive.

### Create an RTP demo video feed

First install gstreamer, for instance on macOS use homebrew:

```
brew install gstreamer
```

With gstreamer available, create an RTP stream using either a dummy generated feed with clock overlay as follows:

```
gst-launch-1.0 videotestsrc ! clockoverlay ! video/x-raw,width=640,height=480 ! videoconvert ! queue ! x264enc tune=zerolatency bitrate=1000 key-int-max=30 ! video/x-h264, profile=constrained-baseline ! rtph264pay pt=96 mtu=1200 ! udpsink host=127.0.0.1 port=6000
```

Or use an actual video source if available (**note:** If using 2-way video, the client will try to use the webcam. This will fail the webcam feed is already taken by the device.):

```
gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,width=640,height=480 ! videoconvert ! queue ! x264enc tune=zerolatency bitrate=1000 key-int-max=30 ! video/x-h264, profile=constrained-baseline ! rtph264pay pt=96 mtu=1200 ! udpsink host=127.0.0.1 port=6000
```

### Create an RTP demo video sink (optional)
In some terminal with gstreamer installed create an RTP sink using:

```
gst-launch-1.0 udpsrc uri=udp://127.0.0.1:6001 ! application/x-rtp, payload=96  ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! xvimagesink
```

### Create an audio feed (optional)
In some terminal with gstreamer installed create an RTP stream using:
```
gst-launch-1.0 -v audiotestsrc wave=sine freq=220 volume=0.01 ! audioconvert ! opusenc ! rtpopuspay name=pay0 pt=111 ! udpsink host=127.0.0.1 port=6002
```

### Create an audio sink (optional)
In some terminal with gstreamer installed create an RTP sink using:
```
gst-launch-1.0 -v udpsrc uri=udp://127.0.0.1:6003 caps="application/x-rtp,media=(string)audio,clock-rate=(int)48000,encoding-name=(string)X-GST-OPUS-DRAFT-SPITTKA-00" ! rtpopusdepay ! opusdec ! audioconvert ! autoaudiosink sync=false
```

## RTSP feeds
The WebRTC example supports getting a video and/or an audio feed from an RTSP server. There is currently no support for two-way audio with RTSP.

For the first connection to the device, it will use RTSP to open an RTP video stream on port 45222 with RTCP on port 45223 and, if the RTSP server has one, an RTP audio stream on port 45224 with RTCP on port 45225. So multiple connections do not clash with eachother, a second connection to the device will use ports 45226-45229 (Connection `n` uses ports `45222+4n - 45222+4n+3`). The UDP socket is bound to `0.0.0.0`, so it will also work with remote RTSP hosts.

The RTSP client only supports one video and one audio stream. If the servers response to the RTSP `Describe` request contains more than one stream of a kind (video/audio), one will be picked and the remaining will be ignored.

### Nabto RTSP demo container
An RTSP server can be started using the Nabto provided RTSP demo container. It is important that the RTSP server and the device example has free network access to each other to enable the UDP streaming sockets to work properly. This is why the example uses `--network host`. **Docker Network host mode does not work on MacOS**.

start the [rtsp-demo-container](https://github.com/nabto/rtsp-demo-container) with:

```
docker run --rm -it --network host rtsp-demo-server
```
**This RTSP demo container does not provide an audio feed so add Audio will not work.** Providing an RTSP server with an audio feed will make one-way audio work.

