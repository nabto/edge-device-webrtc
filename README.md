# Edge Device Webrtc

This WebRTC example implementation for Nabto Embedded SDK allows clients to establish WebRTC connections to the device using Nabto. This WebRTC connection can then be used to stream one-way video and two-way audio feeds from either RTP or RTSP. WebRTC data channels can also be used to invoke CoAP endpoint in the device as well as streaming data through Nabto Streaming.

In addition to WebRTC, this example also implements OAuth to authenticate connections in the IAM module.

## Obtaining the Source

This Github repo references various 3rd party components as submodules. So remember to clone recursively:

```
git clone --recursive https://github.com/nabto/edge-device-webrtc.git
```

## Building
The example is built using cmake from the root of this repo (note that cmake must be invoked twice as shown, this is a known issue, ticket sc-2419):

```
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=`pwd`/install ../
# the above step will fail - ignore this and run the command again
cmake -DCMAKE_INSTALL_PREFIX=`pwd`/install ../
make -j16 install
```

On OSX you may need to turn off `sctp_werror`:
```
cmake -DCMAKE_INSTALL_PREFIX=`pwd`/install -Dsctp_werror=OFF ..
```

## Running

To start the device you must first either have RTP feeds or an RTSP server started. If you do not already have these, see [the sections below](#gstreamer-rtp-feeds) on how to start these.

Before starting the example, you need a device configured in the Nabto Cloud Console. This requires you to configure a fingerprint. To obtain this, run the example application with the `--create-key` argument:

```
$ ./edge_device_webrtc --create-key
Created Raw private key:
  9bde377866f175a4801c1f28e97e3a4faf07cd6889562e8cc187ea00c323c739
With fingerprint:
  badfae3acfa7ab904ac639f0c4cb0ad268d23f4e324e0708aeb567f87da0c324
```

The *Raw key* must be used when starting the example, the *Fingerprint* should be configured in the Nabto Cloud Console.

If you do not already have a device in the Nabto Cloud Console, follow our [general guide for embedded applications](https://docs.nabto.com/developer/platforms/embedded/applications.html#console).

After configuring the device fingerprint, the example device will be able to attach to the Nabto Basestation when started. 
### Running with RTP
Assuming your RTP feeds are running on the default ports, the device is started with:

```
$ ./edge_device_webrtc -d <YOUR_DEVICE_ID> -p <YOUR_PRODUCT_ID> -k <RAW_KEY_CREATED_ABOVE>
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
$ ./edge_device_webrtc -d <YOUR_DEVICE_ID> -p <YOUR_PRODUCT_ID> -k <RAW_KEY_CREATED_ABOVE> --rtsp rtsp://127.0.0.1:8554/video
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

By default, the example expects an RTP video feed on UDP port 6000, an RTP audio feed on UDP port 6001, and an RTP audio sink on UDP port 6002. These ports can be changed using the `--rtp-port arg` argument. The port configured by this argument will be the port of the video feed. The Audio ports will always be the video port plus 1 and 2.

### Create an RTP demo video feed

First install gstreamer, for instance on macOS use homebrew:

```
brew install gstreamer
```

With gstreamer available, create an RTP stream using:

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

## Current limitations
 * `--rtpport` can be used to set the port number of the video video. It is then assumed the audio feed is on `port+1` and that the received audio should be sent to `port+2`;
 * H264 video codec (it is pretty simple to add codecs)
 * PCMU audio codec for RTSP feeds and OPUS audio codec for RTP feeds (Still simple to add/change codecs, but requires code changes to switch)
 * limited RTSP support:
 * - Only supports device sending video/audio feed, no recieving audio
 * - The device will receive RTCP packets, and return dummy Receiver reports as our test cam uses this as keep alive.
 * No RTCP for RTP streams. We have not found any streamers which actually reacts to RTCP, so we do not handle it.


