FROM debian:bookworm as base

RUN apt-get update && apt-get install -y git build-essential autoconf libtool pkg-config gcc libgstrtspserver-1.0-dev libglib2.0-dev libgstreamer1.0-dev gstreamer1.0-plugins-ugly cmake curl zip unzip tar awscli ninja-build
RUN apt-get clean

FROM base as gst-rtsp-launch

WORKDIR /workspace
RUN git clone https://github.com/sfalexrog/gst-rtsp-launch.git

WORKDIR /workspace/gst-rtsp-launch/_build
RUN cmake -GNinja ..
RUN ninja install

FROM base as edge-device-webrtc-build

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

RUN --mount=type=secret,id=ACTIONS_CACHE_URL --mount=type=secret,id=ACTIONS_RUNTIME_TOKEN --mount=type=secret,id=aws,target=/root/.aws/credentials <<EOF
ls /run/secrets
export ACTIONS_CACHE_URL=$(cat /run/secrets/ACTIONS_CACHE_URL); export ACTIONS_RUNTIME_TOKEN=$(cat /run/secrets/ACTIONS_RUNTIME_TOKEN);
printenv
cmake -GNinja -DNABTO_WEBRTC_BUILD_TESTS=OFF ..
ninja install
EOF

from base as runner

COPY --from=gst-rtsp-launch /usr/local /usr/local
COPY --from=edge-device-webrtc-build /usr/local /usr/local

WORKDIR /homedir
COPY demo-scripts/entrypoint.sh /workspace/entrypoint.sh
RUN chmod +x /workspace/entrypoint.sh

ENTRYPOINT ["/workspace/entrypoint.sh"]
