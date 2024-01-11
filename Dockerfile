FROM debian:bookworm

RUN apt-get update && apt-get install -y git build-essential autoconf libtool pkg-config libssl-dev wget vim host gdb ninja-build valgrind sudo clang gcc libcurl4-openssl-dev libgstrtspserver-1.0-dev libglib2.0-dev libgstreamer1.0-dev gstreamer1.0-plugins-ugly

ARG CMAKE_VERSION=3.19.6

WORKDIR /tmp
RUN wget https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz
RUN tar xf cmake-${CMAKE_VERSION}-Linux-x86_64.tar.gz  -C /usr --strip-components=1

WORKDIR /workspace
RUN git clone https://github.com/sfalexrog/gst-rtsp-launch.git

WORKDIR /workspace/gst-rtsp-launch/_build
RUN cmake ..
RUN make -j install

WORKDIR /workspace/device/_build
COPY 3rdparty /workspace/device/3rdparty
COPY examples /workspace/device/examples
COPY include /workspace/device/include
COPY mbedtls-config /workspace/device/mbedtsl-config
COPY nabto-embedded-sdk /workspace/device/nabto-embedded-sdk
COPY src /workspace/device/src
COPY CMakeLists.txt /workspace/device/CMakeLists.txt

RUN cmake .. || cmake ..

RUN make -j4

COPY demo-scripts/entrypoint.sh /workspace/entrypoint.sh
RUN chmod +x /workspace/entrypoint.sh

ENTRYPOINT ["/workspace/entrypoint.sh"]
