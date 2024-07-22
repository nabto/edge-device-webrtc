FROM debian:bullseye

RUN apt-get update && apt-get install -y git build-essential autoconf libtool pkg-config gcc libgstrtspserver-1.0-dev libglib2.0-dev libgstreamer1.0-dev gstreamer1.0-plugins-ugly curl zip unzip tar

WORKDIR /tmp/cmake
RUN curl -sSL https://github.com/Kitware/CMake/releases/download/v3.30.1/cmake-3.30.1-linux-x86_64.sh -o cmake.sh
RUN sh cmake.sh --prefix=/usr/local --skip-license


WORKDIR /workspace
RUN git clone https://github.com/sfalexrog/gst-rtsp-launch.git

WORKDIR /workspace/gst-rtsp-launch/_build
RUN cmake ..
RUN make -j16 install

WORKDIR /workspace/device/_build
COPY 3rdparty /workspace/device/3rdparty
COPY examples /workspace/device/examples
COPY include /workspace/device/include
COPY mbedtls-config /workspace/device/mbedtsl-config
COPY nabto-embedded-sdk /workspace/device/nabto-embedded-sdk
COPY .git /workspace/device/.git
COPY src /workspace/device/src
COPY CMakeLists.txt /workspace/device/CMakeLists.txt
COPY test /workspace/device/test
COPY test-apps /workspace/device/test-apps
COPY vcpkg.json /workspace/device/vcpkg.json

RUN cmake ..

RUN make -j16 install

WORKDIR /homedir
COPY demo-scripts/entrypoint.sh /workspace/entrypoint.sh
RUN chmod +x /workspace/entrypoint.sh

ENTRYPOINT ["/workspace/entrypoint.sh"]
