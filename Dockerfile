FROM debian:bullseye

RUN apt-get update && apt-get install -y git build-essential autoconf libtool pkg-config gcc libgstrtspserver-1.0-dev libglib2.0-dev libgstreamer1.0-dev gstreamer1.0-plugins-ugly cmake curl zip unzip tar

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
