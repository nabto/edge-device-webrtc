# Edge Device Webrtc

This WebRTC example implementation for Nabto Embedded SDK allows clients to establish WebRTC connections to a target device using Nabto. This WebRTC connection can then be used to stream one-way video and two-way audio feeds from either RTP or RTSP. WebRTC data channels can also be used to invoke CoAP endpoints in the device as well as streaming data through Nabto Streaming.

In addition to WebRTC, this example also implements OAuth to authenticate connections in the IAM module.

Note that the example applications use the RTP client in `./src/modules/rtp-client`. This is used to relay RTP packets between a UDP socket and WebRTC. This implementation is based on Unix sockets, and will not work on Windows.

Below, 3 different approaches to building the example application are shown:

1. [Demo Docker container for quick desktop test](#demo-docker-container-for-quick-desktop-test): Get started instantly with no tool or build hassle, you just need Docker installed
2. [Cross building the example for a Linux based camera](#cross-building-the-example-for-a-linux-based-camera): Cross build the example application for Linux based camera using a Docker based build container you can customize with your own specific toolchain
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

## Demo Docker container for quick desktop test

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

## Cross building the example for a Linux based camera

The software is meant to be run on embedded systems such as Linux based cameras,
these cameras often come with their own toolchains and libraries tailored to
the platform.

Dockerfiles are provided in `./cross_build` that demonstrate how to cross compile the example application and all dependencies. For instance, see `cross_build/aarch64/Dockerfile` for an arm64 cross build that works with e.g. Raspberry Pi. To adopt to a custom toolchain, adjust the Dockerfile to include and use the custom toolchain. Assuming the custom toolchain is available in "camera-toolchain.tar.gz", it can be installed it into the image as follows (similar to the `cross_build/sigmastar/Dockerfile` example):

```
RUN apt-get update && apt-get install git build-essential cmake curl file tar -y
COPY camera-toolchain.tar.gz /opt/
RUN tar xfz /opt/camera-toolchain.tar.gz -C /opt/

ENV CC=/opt/gcc-sigmastar-9.1.0-2019.11-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
ENV CXX=/opt/gcc-sigmastar-9.1.0-2019.11-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++
ARG OPENSSL_TARGET=linux-armv4
```
The `OPENSSL_TARGET` is typically `linux-aarch64` for 64-bit ARM based targets and `linux-armv4` for 32-bit.

The cross build example consists of a Dockerfile which uses several layers to
build the individual dependency libraries and then builds it into a binary.

The build can be run as follows:

```
docker build -f cross_build/sigmastar/Dockerfile --progress plain -t edge_device_webrtc_sigmastar .
```

> [!IMPORTANT]
> You need to call the command from this directory to ensure the correct context is provided for docker.

To access the build output, create an instance of the container and copy the binary to the current directory on the host machine:

```
$ docker create --name tmp_container edge_device_webrtc_sigmastar
$ docker cp tmp_container:/tmp/install/bin/edge_device_webrtc .
$ file edge_device_webrtc
edge_device_webrtc: ELF 32-bit LSB executable, ARM, EABI5 version 1 (GNU/Linux), dynamically linked, interpreter /lib/ld-linux-armhf.so.3, BuildID[sha1]=c85e0ee63d691499630efeae4e2b00e22068cc22, for GNU/Linux 3.2.0, with debug_info, not stripped
```

#### Testing the cross build without a physical target

The aarch64 Dockerfile (`cross_build/aarch64/Dockerfile`) has a commented out section at the bottom that shows how to run the resulting aarch64 binary through qemu; this demonstrates that the compiled binary works on something else than the system used to compile the binary.

If enabling the qemu section at the bottom of `cross_build/aarch64/Dockerfile`, the resulting aarch64 binary can be run by starting and interactive session with `docker run --rm -it edge_device_webrtc_aarch64`. Then the aarch64 binary can be run as `LD_LIBRARY_PATH=/tmp/example qemu-aarch64-static /tmp/example/edge_device_webrtc`.

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

### Building
The example is built using cmake from the root of this repo:

```
cmake --workflow --preset release
```

> [!IMPORTANT]
> If your build host has limited resources and you try to limit the build concurrency using e.g. `make -j1`, you also need to explicitly limit the vcpkg build concurrency using `export VCPKG_MAX_CONCURRENCY=1` prior to running cmake.


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

### Crosscompiling

It is easiest to crosscompile just using the vcpkg toolchain.

For example to crosscompile for arm64 on linux it is only neccessary to use the following options
```
export CC=aarch64-linux-gnu-gcc
export CXX=aarch64-linux-gnu-g++
mkdir -p build/linux_arm64_crosscompile
cd build/aarch64
cmake -DCMAKE_TOOLCHAIN_FILE=`pwd`/../../3rdparty/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_MODULE_PATH=`pwd`/../../cmake/vcpkg -DVCPKG_TARGET_TRIPLET=arm64-linux -DCMAKE_INSTALL_PREFIX=`pwd`/install ../../
make install
```

The above example can also be run using `cmake --workflow --preset linux_arm64_crosscompile`

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

### H264+PCMU feed from FIFO File Descriptor

The demo can read a raw H264 video and PCMU audio feeds from FIFO file descriptors. This feature is advanced usage and comes with several limitations, so you must read the documentation carefully before using this. As such, this section will assume you are familiar with using the demo from more basic usage.

**limitations:**
 * This example is based on unix file descriptors
 * The example feeds requires Gstreamer or FFMPEG installed
 * The example video feed is based on v4l2 webcam at `/dev/video0`, but can be any video source.
 * The example audio feed is based on pulsesrc, but can be any audio source.
 * The example only supports sending a H264 video feed and a PCMU audio feed. No downstream feeds are supported.
 * The video feed must use the byte-stream format specified in Annex B of the [ITU-T H.264 Recommendation](https://www.itu.int/rec/T-REC-H.264-202108-I/en).
 * The example will assume a fifo video feed is present, but audio is optional. This means the device cannot be started with `--fifo-audio ..` without also using `--fifo`


A test video feed can be created using Gstreamer after creating the FIFO file descriptor:

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

Similarly an audio feed can be started using:

```
mkfifo /tmp/audio.fifo
gst-launch-1.0 -v  pulsesrc ! audio/x-raw, format=S16LE,channels=1,rate=8000 ! audioresample ! mulawenc ! filesink buffer-mode=2 location="/tmp/audio.fifo"
```

The device can be started using the file path:

```
./examples/webrtc-demo/edge_device_webrtc -k <KEY> -d <DEVICE_ID> -p <PRODUCT_ID> --fifo /tmp/video.fifo --fifo-audio /tmp/audio.fifo
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
