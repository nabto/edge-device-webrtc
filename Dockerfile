FROM debian:trixie

RUN apt-get update && apt-get install -y git build-essential autoconf libtool pkg-config gcc libgstrtspserver-1.0-dev libglib2.0-dev libgstreamer1.0-dev gstreamer1.0-plugins-ugly curl zip unzip tar ninja-build

WORKDIR /tmp/cmake
ARG CMAKE_VERSION=4.4.5
RUN curl -sSL https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.sh -o cmake.sh
RUN sh cmake.sh --prefix=/usr/local --skip-license


WORKDIR /workspace
ARG GST_RTSP_LAUNCH_VERSION=f466b3f4d3b62d847344ae0aa86531c4aec266bb
RUN git clone https://github.com/sfalexrog/gst-rtsp-launch.git && git -C gst-rtsp-launch checkout ${GST_RTSP_LAUNCH_VERSION}

WORKDIR /workspace/gst-rtsp-launch/_build
RUN cmake ..
RUN make -j16 install

WORKDIR /workspace/device
COPY 3rdparty /workspace/device/3rdparty
COPY examples /workspace/device/examples
COPY include /workspace/device/include
COPY mbedtls-config /workspace/device/mbedtsl-config
COPY .git /workspace/device/.git
COPY src /workspace/device/src
COPY CMakeLists.txt /workspace/device/CMakeLists.txt
COPY test /workspace/device/test
COPY test-apps /workspace/device/test-apps
COPY vcpkg.json /workspace/device/vcpkg.json
COPY vcpkg-configuration.json /workspace/device/vcpkg-configuration.json
COPY build-scripts /workspace/device/build-scripts
COPY cmake /workspace/device/cmake
COPY cmake-scripts /workspace/device/cmake-scripts
COPY CMakePresets.json /workspace/device/CMakePresets.json

RUN cmake --workflow --preset release

RUN mkdir -p /usr/local/bin
RUN cp build/release/install/bin/edge_device_webrtc /usr/local/bin/
RUN cp build/release/install/bin/simple_webrtc_device /usr/local/bin/

WORKDIR /homedir
COPY demo-scripts/entrypoint.sh /workspace/entrypoint.sh
RUN chmod +x /workspace/entrypoint.sh

ENTRYPOINT ["/workspace/entrypoint.sh"]
