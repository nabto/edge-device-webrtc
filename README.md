# Edge Device Webrtc

This WebRTC example implementation for Nabto Embedded SDK allows clients to establish WebRTC connections to a target device using Nabto. This WebRTC connection can then be used to stream one-way video and two-way audio feeds from either RTP or RTSP. WebRTC data channels can also be used to invoke CoAP endpoints in the device as well as streaming data through Nabto Streaming.

In addition to WebRTC, this example also implements OAuth to authenticate connections in the IAM module.

Note that the example applications use the RTP client in `./src/modules/rtp-client`. This is used to relay RTP packets between a UDP socket and WebRTC. This implementation is based on Unix sockets, and will not work on Windows.

Below, 3 different approaches to building the example application are shown:

1. [Demo Docker container for quick desktop test](#demo-docker-container-for-quick-desktop-test): Get started instantly with no tool or build hassle, you just need Docker installed
2. [Cross building for Linux based cameras](#cross-building-the-example-for-linux-based-cameras): Cross build the example application for Linux based cameras using a Docker based build container you can customize with your own specific toolchain
3. [Building the example for desktop](#building-the-example-for-desktop): Build the example application using build tools in your development environment

> [!TIP]
> **To build a binary to be installed on a camera, pick [approach 2](#cross-building-the-example-for-a-linux-based-camera)**.

You can see how to run the resulting executable from approaches 2 and 3 in the [Running](#running-the-example) section.

For detailed documentation, including integration guidelines, motivation and background information, please refer to our official [documentation site](https://docs.nabto.com/developer/guides/webrtc/intro.html).


## Obtaining the Source

This Github repo references various 3rd party components as submodules. So remember to clone recursively:

```
git clone --recursive https://github.com/nabto/edge-device-webrtc.git
```

# Demo Docker container for quick desktop test

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

Too see how to build the example for a desktop system, see section [Build example for desktop]

# Cross building for Linux based cameras

The software is meant to be run on embedded systems such as Linux based cameras,
these cameras often come with their own toolchains and libraries tailored to
the platform.

It is easiest to cross compile using the vcpkg package manager. But for more
advanced builds is is also possible to cross compile the software without using
the VCPKG package manager at all, then you just need to provide all the packages
your self, see e.g. the folder ./cross_build/aarch64

For example to crosscompile for 64 bit ARM on linux it is only neccessary to use the following options
```
export CC=aarch64-linux-gnu-gcc
export CXX=aarch64-linux-gnu-g++
mkdir -p build/linux_arm64_crosscompile
cd build/linux_arm64_crosscompile
cmake -DCMAKE_TOOLCHAIN_FILE=`pwd`/../../3rdparty/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_MODULE_PATH=`pwd`/../../cmake/vcpkg -DVCPKG_TARGET_TRIPLET=arm64-linux -DCMAKE_INSTALL_PREFIX=`pwd`/install ../../
make install
```

The option `-DVCPKG_TARGET_TRIPLET` is used to specify the triplet which VCPKG
uses to build the 3rdparty dependencies that EdgeDeviceWebRTC depends on read
more here: https://learn.microsoft.com/en-us/vcpkg/concepts/triplets or find a
list of commonly used triplets here:
https://github.com/microsoft/vcpkg/tree/master/triplets

# Building for the Raspberry PI

The Raspberry PI boards commonly runs the Raspbian OS, this OS is either running
as 32bit or 64bit.

This software can be built for the Raspberry PI either as a crosscompilation or
a compilation directly on the target. Building the software on the RPI takes a
long time since it is slow at compiling software so the easiest approach is to
cross compile the software for the Raspberry PI.

## Cross compiling for Raspbian OS.

This guide assumes you have a Debian or Ubuntu based machine to build the software on.

This guide does not build the software for Raspberry PI 1 and Raspberry PI Zero
as they are running ARMv6 which is not the target for the 32 bit build.

Install prerequisites:

```
sudo apt-get install cmake git ninja-build build-essential curl zip unzip tar pkg-config g++-arm-linux-gnueabihf gcc-arm-linux-gnueabihf g++-aarch64-linux-gnu gcc-aarch64-linux-gnu
```

Build for 32bit:

```
cmake --workflow --preset linux_arm_crosscompile
```

Build for 64bit:

```
cmake --workflow --preset linux_arm64_crosscompile
```

After the build the 32bit edge_device_webrtc executable is found at
`build/linux_arm_crosscompile/install/bin/edge_device_webrtc` and the 64bit
executable can be found at
`build/linux_arm64_crosscompile/install/bin/edge_device_webrtc`

The resulting binary can be copied and run on Raspberry PI.

## Building the example for desktop

If building for the desktop, e.g. for development or testing without Docker, build scripts are provided that build dependencies through [vcpkg](https://vcpkg.io/en/).

> [!IMPORTANT]
> For embedded systems, it is simpler to build using a Docker container as outlined above. The default vcpkg build will fail for embedded systems and is not supported.

### Tools
You need the following tools to build the example:

* Git
* [CMake](https://cmake.org/)
* C++ compiler
* curl zip unzip tar
* pkg-config

Install requirements on debian based linux systems:
```
apt-get install cmake git ninja-build build-essential curl zip unzip tar pkg-config
```

### Building
The example is built using cmake from the root of this repo:

```
cmake --workflow --preset release
```

The resulting binaries and libraries are located in the folder `./build/release/install`.

## Running the example

To start the device you must first either have RTP feeds or an RTSP server started. If you do not already have these, we have [guides available](https://docs.nabto.com/developer/platforms/embedded/linux-ipc/webrtc-example.html#feeds) for starting both simulated RTP and RTSP feeds.

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

To connect to your device, follow the `Initial user pairing link` printed by the device when first started. This will open our [WebRTC demo website](https://demo.smartcloud.nabto.com/) and initiate pairing with the device. The process is described in more detail [in the documentation](https://docs.nabto.com/developer/guides/webrtc/quickstart.html#website). Alternatively, instead of using the full website, you can use the simple [standalone static web page](https://docs.nabto.com/developer/platforms/js/webrtc-example.html).


### Other connect options

The device can also be accessed through Android/iOS Apps, Google Home, and Alexa. These functionalities are not yet finalized. For more information about these features, contact Nabto support.


## Advanced build topics

### Testing the RTSP client
When building with unit tests, CMake will look for GStreamer dependencies. If it finds all required dependencies, it will include the RTSP client tests in the unit test executable. If any dependency is missing, these tests will simply not be included in the unit test executable. On Debian, the dependencies are installed with these apt packages:

* `libgstrtspserver-1.0-dev`
* `libglib2.0-dev`
* `libgstreamer1.0-dev`
* `gstreamer1.0-plugins-ugly`

Additionally, the RTSP client supports both Basic and Digest authentication. These features can be disabled with the CMake options `-DRTSP_HAS_BASIC_AUTH=OFF` and `-DRTSP_HAS_DIGEST_AUTH=OFF`.

### Building without vcpkg

Vcpkg is great when used to build standard software for common platforms such as
Windows, Linux, Mac, iOS and Android. The usage of vcpkg can be disabled when
invoking cmake by setting `NABTO_WEBRTC_USE_VCPKG` e.g. `cmake
-DNABTO_WEBRTC_USE_VCPKG=OFF ...`. When vcpkg is disabled it is up to the
builder to provide the needed libraries such as openssl, curl and boost test.
The libraries needed depends on which configurations of the software is being
built.

There is an example in the folder `cross_build/aarch64` which compiles all the
dependencies manually.

### Building without tests

Tests in the library depends on boot test, to remove this dependency the
software can be compiled without tests, this can be done using the cmake flag
"NABTO_WEBRTC_BUILD_TESTS" e.g. `cmake -DNABTO_WEBRTC_BUILD_TESTS=OFF ...`

### Building without Curl

Curl is used in the RTSP client and when validating OAuth tokens. It's currently
not configurable to build the project without the curl dependency, but the
software components defined inside `nabto/nabto_device_webrtc.hpp` does not
depend on libcurl. It is currently not possible to build the software without
libcurl being present. It is possible to build a binary which does not depend on
libcurl. e.g. a binary which does not validate oauth tokens or use the rtsp
client.

### Building without JWT
JWT-cpp is used when validating OAuth tokens in the `edge_device_webrtc` example. It is currently not configurable to build the project without this dependency.

The RTSP client module also depends on JWT-cpp for base64 encoding in Basic authentication. This can be disabled with the CMake option `-DRTSP_HAS_BASIC_AUTH=OFF`

### Building without OpenSSL

OpenSSL is used when validating OAuth tokens, when challenge response validating
device fingerprints and in libdatachannels dependencies. It is currently not
configurable to build the project without OpenSSL. The components defined inside
`nabto/nabto_device_webrtc.hpp` does not directly depend on OpenSSL only
through dependencies which can be configured to use e.g. MbedTLS instead.

The RTSP client module depends on OpenSSL for digest authentication. This can be disabled with the CMake option `-DRTSP_HAS_DIGEST_AUTH=OFF`

### Building without CMake

If the software needs to be built without using CMake, the source files and
dependencies needs to be defined seperately in whatever buildsystem which is
then used. This is out of scope for this documentation.

### H264 feed from FIFO File Descriptor

The demo can read a raw H264 feed from a FIFO file descriptor. This feature is advanced usage and comes with several limitations, so you must read the documentation carefully before using this. As such, this section will assume you are familiar with using the demo from more basic usage.

**limitations:**
 * This example is based on unix file descriptors
 * The example feeds requires Gstreamer or FFMPEG installed
 * The example feeds are based on v4l2 webcam at `/dev/video0`, but can be any video source.
 * The example only supports sending a H264 video feed. No downstream video, and no audio.
 * The feed must use the byte-stream format specified in Annex B of the [ITU-T H.264 Recommendation](https://www.itu.int/rec/T-REC-H.264-202108-I/en).


A test feed can be created using Gstreamer after creating the FIFO file descriptor:

```
mkfifo /tmp/video.fifo
gst-launch-1.0 videotestsrc ! clockoverlay ! video/x-raw,width=640,height=480 ! videoconvert ! queue !  x264enc tune=zerolatency ! filesink buffer-size=16 buffer-mode=2 location="/tmp/video.fifo"
```

Or using FFMPEG:

```
mkfifo /tmp/video.fifo
ffmpeg -f video4linux2 -s vga -i /dev/video0 -f h264 -tune zerolatency file:///tmp/video.fifo
```

FFMPEG will ask if you want to overwrite the file, choose yes.

The device can be started using the file path:

```
./examples/webrtc-demo/edge_device_webrtc -k <KEY> -d <DEVICE_ID> -p <PRODUCT_ID> -f /tmp/video.fifo
```

You can now connect to the device from a client and see the feed.
The Gstreamer/FFMPEG commands will only write to the FIFO when the device is reading it. This also means when the client connection is closed and the device stops reading the FIFO, the Gstreamer/FFMPEG will exit and new client connections will not be able to see the feed before the streamer is started again (`mkfifo` does not need to be run again, only the streamer command).


## Building the NabtoWebRTCSDK components

This library consists of several components each component is built as a library
and depends on other libraries.

A user of the NabtoWebRTC libraries is in charge of defining which 3rdparty
libraries and their versions which the NabtoWebRTC libraries should use. Often
such decisions is handled by a package manager such as apt, vcpkg, etc.

Depending on the configuration of the NabtoWebRTCSDK the following third party
libraries is needed:

  * NabtoEmbeddedSDK
  * MbedTLS
  * OpenSSL
  * Curl
  * jwt-cpp
  * cxxopts
  * plog
  * LibDataChannel

Each of these dependencies has to be provided by the consumer of this library.
Each of these libraries can have dependencies themselves.


## Linking the raw libraries into an executable

An example of such a linking can be seen in the folder
`test/distribution/direct_link_gcc` and
`test/distribution/direct_link_webrtc_demo`
