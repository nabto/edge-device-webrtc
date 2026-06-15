FROM --platform=linux/amd64 debian:bookworm

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        git ca-certificates build-essential cmake ninja-build pkg-config \
        curl zip unzip tar xz-utils file python3 autoconf automake libtool bison \
    && rm -rf /var/lib/apt/lists/*

ARG TOOLCHAIN_TARBALL=nabto-sigmastar-libraries/arm-sigmastar-linux-uclibcgnueabihf-9.1.0.tar.xz
ARG TOOLCHAIN_DIR=arm-sigmastar-linux-uclibcgnueabihf-9.1.0
COPY ${TOOLCHAIN_TARBALL} /opt/toolchain.tar.xz
RUN tar -xJf /opt/toolchain.tar.xz -C /opt && rm /opt/toolchain.tar.xz
ENV PATH=/opt/${TOOLCHAIN_DIR}/bin:$PATH
ENV CC=arm-sigmastar-linux-uclibcgnueabihf-gcc
ENV CXX=arm-sigmastar-linux-uclibcgnueabihf-g++

# Sanity-check the toolchain can produce ARM ELFs before the long build
RUN printf 'int main(void){return 0;}\n' > /tmp/h.c && \
    arm-sigmastar-linux-uclibcgnueabihf-gcc /tmp/h.c -o /tmp/h && \
    file /tmp/h | grep -q "ARM"

# vcpkg needs the host to build its tooling
ENV VCPKG_FORCE_SYSTEM_BINARIES=1

WORKDIR /src
COPY edge-device-webrtc /src/edge-device-webrtc

WORKDIR /src/edge-device-webrtc
RUN set -e; \
    mkdir -p build/sigmastar; cd build/sigmastar; \
    if ! cmake \
        -DCMAKE_TOOLCHAIN_FILE=/src/edge-device-webrtc/3rdparty/vcpkg/scripts/buildsystems/vcpkg.cmake \
        -DCMAKE_MODULE_PATH=/src/edge-device-webrtc/cmake/vcpkg \
        -DVCPKG_OVERLAY_TRIPLETS=/src/edge-device-webrtc/triplets \
        -DVCPKG_OVERLAY_PORTS=/src/edge-device-webrtc/ports \
        -DVCPKG_TARGET_TRIPLET=arm-sigmastar-linux \
        -DCMAKE_BUILD_TYPE=MinSizeRel \
        -DCMAKE_C_FLAGS="-ffunction-sections -fdata-sections -g" \
        -DCMAKE_CXX_FLAGS="-ffunction-sections -fdata-sections -g" \
        -DCMAKE_EXE_LINKER_FLAGS="-Wl,--gc-sections" \
        -DCMAKE_SHARED_LINKER_FLAGS="-Wl,--gc-sections" \
        -DNABTO_WEBRTC_BUILD_TESTS=OFF \
        -DCMAKE_INSTALL_PREFIX=/src/edge-device-webrtc/build/sigmastar/install \
        ../../ ; then \
        echo "=== vcpkg manifest install log ==="; \
        cat vcpkg-manifest-install.log || true; \
        echo "=== buildtrees error logs ==="; \
        find /src/edge-device-webrtc/3rdparty/vcpkg/buildtrees -maxdepth 2 -type f \
            \( -name "install-*-out.log" -o -name "install-*-err.log" -o -name "build-*-err.log" \) \
            -newer /src/edge-device-webrtc/3rdparty/vcpkg/bootstrap-vcpkg.sh \
            -print -exec tail -n 200 {} \; ; \
        exit 1; \
    fi && \
    make -j"$(nproc)" install && \
    mkdir -p install/debug && \
    cd install/bin && \
    for f in *; do \
        arm-sigmastar-linux-uclibcgnueabihf-objcopy --only-keep-debug "$f" "../debug/$f.debug"; \
        arm-sigmastar-linux-uclibcgnueabihf-strip --strip-all "$f"; \
        arm-sigmastar-linux-uclibcgnueabihf-objcopy --add-gnu-debuglink="../debug/$f.debug" "$f"; \
        chmod -x "../debug/$f.debug"; \
    done && \
    cd .. && \
    echo "=== stripped binaries (ship to /app) ===" && ls -lh bin && \
    echo "=== detached debug info (keep off-device) ===" && ls -lh debug && \
    file bin/edge_device_webrtc
