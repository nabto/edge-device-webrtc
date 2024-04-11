FROM debian:bookworm

RUN apt-get update && apt-get install -y git build-essential autoconf libtool pkg-config gcc libgstrtspserver-1.0-dev libglib2.0-dev libgstreamer1.0-dev gstreamer1.0-plugins-ugly cmake curl zip unzip tar awscli

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

ARG VCPKG_BINARY_SOURCES=default
ENV VCPKG_BINARY_SOURCES=${VCPKG_BINARY_SOURCES}

RUN echo "VCPKG_BINARY_SOURCES=${VCPKG_BINARY_SOURCES}"

RUN --mount=type=secret,id=ACTIONS_CACHE_URL --mount=type=secret,id=ACTIONS_RUNTIME_TOKEN <<EOF
ls /run/secrets
cat /run/secrets/ACTIONS_CACHE_URL
cat /run/secrets/ACTIONS_RUNTIME_TOKEN
export ACTIONS_CACHE_URL=$(cat /run/secrets/ACTIONS_CACHE_URL); export ACTIONS_RUNTIME_TOKEN=$(cat /run/secrets/ACTIONS_RUNTIME_TOKEN);
printenv
cmake ..
make -j16 install
EOF

WORKDIR /homedir
COPY demo-scripts/entrypoint.sh /workspace/entrypoint.sh
RUN chmod +x /workspace/entrypoint.sh

ENTRYPOINT ["/workspace/entrypoint.sh"]
