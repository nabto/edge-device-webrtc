# Edge Device Webrtc

This WebRTC example implementation for Nabto Embedded SDK allows clients to establish WebRTC connections to a target device using Nabto. This WebRTC connection can then be used to stream one-way video and two-way audio feeds. Audio and video can be ingested from RTP and RTSP sources as well as raw streams through filesystem FIFOs. WebRTC data channels can also be used to invoke CoAP endpoints in the device as well as streaming data through Nabto Streaming.

In addition to WebRTC, this example also implements OAuth to authenticate connections in the IAM module.

Note that the example applications use the RTP client in `./src/modules/rtp-client`. This is used to relay RTP packets between a UDP socket and WebRTC. This implementation is based on Unix sockets, and will not work on Windows.

The example application can be tried as follows:

1. [Quick desktop test using pre-built Docker image](#quick-desktop-test-using-pre-built-docker-image): Get started instantly with no tool or build hassle, you just need Docker installed

2. [Build for desktop systems](#build-for-desktop-systems)

3. [Build on Raspberry Pi](#build-on-raspberry-pi)

4. [Cross-build for Raspberry Pi](#cross-build-for-raspberry-pi)

5. [Cross-build for embedded systems](#cross-build-for-embedded-systems): Build the example application and all dependencies using [`vcpkg`](https://vcpkg.io/en/) for dependency management. **_CHOOSE THIS APPROACH IF BUILDING FOR A LINUX BASED CAMERA_**

6. [Provide dependencies manually](#provide-dependencies-manually): Build the example application and provide all dependencies yourself instead of using `vcpkg`. For advanced use only.

You can see how to run the resulting executables in the [Running](#running-the-example) section.

> [!TIP]
> **To build a binary to be installed on a camera, pick approach 5, [Cross-build for embedded systems](#cross-build-for-embedded-systems)**.

For detailed documentation, including integration guidelines, motivation and background information, please refer to our official [documentation site](https://docs.nabto.com/developer/guides/webrtc/intro.html).

## Obtaining the Source

This Github repo uses [`vcpkg`](https://vcpkg.io/en/) through a submodule. So if using approach 2 above, remember to clone recursively:

```
git clone --recursive https://github.com/nabto/edge-device-webrtc.git
```

# Quick desktop test using pre-built Docker image

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

# Building

## Prerequisites

You will need the following tools to build the WebRTC example application (note: see [special prerequisites](tbd) if building _on_ a Raspberry Pi):

* Git
* [CMake](https://cmake.org/)
* [Ninja](https://github.com/ninja-build/ninja/wiki/Pre-built-Ninja-packages)
* curl
* zip
* unzip
* tar
* pkg-config

And then of course a relevant toolchain and/or [cross-toolchain](https://en.wikipedia.org/wiki/Cross_compiler). Most of the above (except perhaps the toolchain) can be installed using the platform standard package manager (like `apt` on Debian flavor Linux systems) or defacto standard package manager (like `homebrew` on macOS). Note that cross-compilation is often not well supported on modern macOS systems, even through Docker, due to toolchains being x86 based.

For instance, to cross-build on a Debian system for 64-bit ARM system, you can install all dependencies as follows:

```
sudo apt-get install cmake git ninja-build build-essential curl zip unzip tar pkg-config \
   g++-aarch64-linux-gnu gcc-aarch64-linux-gnu
```

If your toolchain is not available from a package manager, if e.g. you have gotten it from a video chipset vendor, you must manually install it and learn how to invoke the C and C++ compiler from the vendors' instructions. Then set the `CC` and `CXX` environment variables, respectively, as outlined below.

## Build for desktop systems

> [!TIP]
> **To build a binary to be installed on a camera, instead see the [cross-build section](#cross-build-for-embedded-systems)**.

This section describes how to build for desktop Linux, macOS and Windows systems. The purpose is to build for testing and development in a simpler environment than an actual embedded system. If you want to build software to use _on_ an embedded system, please see the [cross-build](#cross-build-for-embedded-systems) section.

First install prerequisites as outlined [above](#prerequisites).

Then simply execute:

```
cmake --workflow --preset release
```

After a successful build, the resulting binary can be found here:

```
./build/release/install/bin/edge_device_webrtc
```

## Build on Raspberry Pi

The Raspberry Pi boards commonly runs the Raspbian OS, this OS is either running
as 32-bit or 64-bit. We have precompiled binaries available under [releases in github](https://github.com/nabto/edge-device-webrtc/releases).

The Raspberry PI is incredibly slow when using it for compiling software, but
often it is the simplest approach if a linux machine is not available for
cross-compiling the software on. To speedup the compilation, this compilation
depends on system libraries boost curl and openssl instead of compiling all the
dependencies.

For a much faster build process, we also support a simple [cross-build for Raspberry Pi](#cross-compiling-for-raspbian-os).

### Prerequisites

Install the build tools:
```
sudo apt-get install cmake git ninja-build build-essential curl zip unzip tar pkg-config
```

Install the libraries:
```
sudo apt-get install libcurl4-openssl-dev libssl-dev libboost-test-dev
```

Build the software:
```
cmake --workflow --preset raspberry_pi_native
```

The resulting `edge_device_webrtc` binary is located at `build/raspberry_pi_native/install/bin/edge_device_webrtc`


## Cross-build for Raspberry Pi

This guide assumes you have a Debian or Ubuntu based machine to crossbuild the software on.

This guide does not build the software for Raspberry Pi 1 and Raspberry Pi Zero
as they are running ARMv6 which is not the target for the 32 bit build.

First install prerequisites, the [build prequisites](#prerequisites) section has an exact example for Raspberry Pi you can use.

We have prepared specifc targets for a simple build experience.

Build for 32-bit ARM, including Raspberry Pi 32-bit:

```
cmake --workflow --preset linux_arm_crosscompile
```

Build for 64-bit ARM, including Raspberry Pi 64-bit:

```
cmake --workflow --preset linux_arm64_crosscompile
```

After the build, the 32-bit `edge_device_webrtc` executable is found at
`build/linux_arm_crosscompile/install/bin/edge_device_webrtc` and the 64-bit
executable can be found at
`build/linux_arm64_crosscompile/install/bin/edge_device_webrtc`

## Cross-build for embedded systems

Our build system is based on `vcpkg`. This is an open-source package manager designed to simplify managing C/C++ libraries on various platforms. It simplifies downloading, building and integrating dependencies into projects, reducing the complexity of managing libraries manually and ensuring consistency across different environments. [Read more about vcpkg](https://vcpkg.io/en/). As an alternative to this standard build system, you can [provide all dependencies yourself](#provide-dependencies-manually).

`vcpkg` uses [_triplets_](https://learn.microsoft.com/en-us/vcpkg/concepts/triplets) to define the target architecture, platform and library linkage which is crucial for cross-compiling dependencies for embedded systems. A triplet looks like `arm64-linux-dynamic`. The linkage configuration is often omitted and a default is used based on the specified architecture and platform. So to cross compile for e.g. an ARM 64-bit based Linux system, you often just specify `arm64-linux`.

The triplet is specified to cmake as follows:

```
-DVCPKG_TARGET_TRIPLET=arm64-linux
```

In addition to configuring `vcpkg` with a triplet, you must point the build system to the C and C++ compiler for the target platform through the environment variables `CC` and `CXX`.

The full build command for building for an ARM 64-bit based linux system then looks as follows:

```
export CC=aarch64-linux-gnu-gcc
export CXX=aarch64-linux-gnu-g++
mkdir -p build/linux_arm64_crosscompile
cd build/linux_arm64_crosscompile
cmake -DCMAKE_TOOLCHAIN_FILE=`pwd`/../../3rdparty/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DCMAKE_MODULE_PATH=`pwd`/../../cmake/vcpkg \
      -DVCPKG_TARGET_TRIPLET=arm64-linux \
      -DCMAKE_INSTALL_PREFIX=`pwd`/install ../../
make install
```

### Choosing an existing triplet

The following table shows official and community triplets often relevant for IP cameras:

| Architecture | Description                                  | Suggested Triplet           |
|--------------|----------------------------------------------|-----------------------------|
| `armhf`      | ARM 32-bit with hardware floating-point      | `arm-linux`                 |
| `arm64`      | ARM 64-bit                                   | `arm64-linux`               |
| `mips64`     | MIPS 64-bit                                  | `mips64-linux`              |

So to build for an ARM 32-bit linux system, change the triplet configuration in the cmake command in the previous section as follows:

```
-DVCPKG_TARGET_TRIPLET=armhf-linux
```

Also update `CC` and `CXX` as needed.

### Using a custom triplet for architectures not officially supported

If you need to build for an architecture that is not officially supported or part of the community repository, it is quite straight forward to make your own. For instance, to support `mips32`, create a `triplets` subdirectory in the repo root (at the same level as `vcpkg.json`).

```
mkdir triplets
```

In that directory, create a file named `mips32-linux.cmake` with the following contents:

```
set(VCPKG_TARGET_ARCHITECTURE mips32)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
```

Then add `-DVCPKG_OVERLAY_TRIPLETS=<path>/triplets` to your build command and select the new target triplet with `-DVCPKG_TARGET_TRIPLET=mips32-linux`:

```
export CC="mips-gcc720-glibc226/bin/mips-linux-gnu-gcc -muclibc"
export CXX="mips-gcc720-glibc226/bin/mips-linux-gnu-g++ -muclibc"
mkdir -p build/linux_mips32_crosscompile
cd build/linux_mips32_crosscompile
cmake -DCMAKE_TOOLCHAIN_FILE=`pwd`/../../3rdparty/vcpkg/scripts/buildsystems/vcpkg.cmake \
      -DCMAKE_MODULE_PATH=`pwd`/../../cmake/vcpkg \
      -DVCPKG_OVERLAY_TRIPLETS=`pwd`/../../triplets \
      -DVCPKG_TARGET_TRIPLET=mips32-linux \
      -DCMAKE_INSTALL_PREFIX=`pwd`/install ../../
make install
```

## Provide dependencies manually

It is easiest to cross compile using the `vcpkg` package manager (see [above](#cross-build-for-embedded-systems)) as all dependencies are then built automatically. But for more advanced builds, it is also possible to cross-compile the software without using the `vcpkg` package manager at all, then you just need to provide all the dependencies yourself, including openssl, curl and boost.

The specific libraries needed depends on which configurations of the software is being built. See the [advanced section](#advanced-build-topics) for how to disable specific features - if a feature is not needed, you may skip some dependencies.

To bring your own dependencies and disable `vcpkg`, invoke cmake by setting `NABTO_WEBRTC_USE_VCPKG`:

```
cmake -DNABTO_WEBRTC_USE_VCPKG=OFF ...
```

For an example of such build, see the Docker based build in the folder `./cross_build/aarch64`.

# Running the example

To start the device you must first either have RTP feeds or an RTSP server started. Or have a direct H264 and/or audio source that can be [fed through a fifo](#h264-feed-from-fifo-file-descriptor). If you do not have any video source but would like to try this WebRTC example anyway, we have [guides available](https://docs.nabto.com/developer/platforms/embedded/linux-ipc/webrtc-example.html#feeds) for starting both simulated RTP and RTSP feeds.

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
 * The example supports a one-way H264 video feed and a two-way PCMU audio feed.
 * The video feed must use the byte-stream format specified in Annex B of the [ITU-T H.264 Recommendation](https://www.itu.int/rec/T-REC-H.264-202108-I/en).
 * The example will assume a fifo video feed is present, but audio is optional. This means the device cannot be started with `--fifo-audio ..` without also using `--fifo`
 * The example assumes the audio feed from the client should be written to the file: `<--fifo-audio option>.out` eg. if `--fifo-audio foo.fifo` it will write to `foo.fifo.out`.


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

Downstream audio fifo should also be created and can then be played back:

```
mkfifo /tmp/audio.fifo.out
gst-launch-1.0 filesrc location="/tmp/audio.fifo.out" ! queue ! audio/x-mulaw, rate=8000, channels=1 ! mulawdec ! audioconvert ! autoaudiosink sync=false
```

The device can be started using the file path:

```
./examples/webrtc-demo/edge_device_webrtc -k <KEY> -d <DEVICE_ID> -p <PRODUCT_ID> --fifo /tmp/video.fifo --fifo-audio /tmp/audio.fifo
```

You can now connect to the device from a client and see the feed.
The Gstreamer/FFMPEG commands will only write to the FIFO when the device is reading it. This also means when the client connection is closed and the device stops reading the FIFO, the Gstreamer/FFMPEG will exit and new client connections will not be able to see the feed before the streamer is started again (`mkfifo` does not need to be run again, only the streamer command).

To use the 2-way audio feature, you must use a client with 2-way audio support. The demo website does NOT have this support. For a browser client, use the `examples/webrtc-demo/web-client`. With this client, once you have opened a connection and the feed from the device, you must click `Add Upstream Audio` before the browser will start sending audio data to the device.


## Building the NabtoWebRTCSDK components

This library consists of several components each component is built as a library
and depends on other libraries.

A user of the NabtoWebRTC libraries is in charge of defining which 3rdparty
libraries and their versions which the NabtoWebRTC libraries should use. Often
such decisions is handled by a package manager such as apt, `vcpkg`, etc.

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
