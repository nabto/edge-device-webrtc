FROM debian:bookworm AS base

RUN apt-get update && apt-get install git build-essential cmake gcc-aarch64-linux-gnu g++-aarch64-linux-gnu curl file tar unzip -y

ENV CC=aarch64-linux-gnu-gcc
ENV CXX=aarch64-linux-gnu-g++
ARG OPENSSL_TARGET=linux-aarch64

FROM base AS mbedtls

RUN apt-get update && apt-get install python3 -y

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v3.5.2.tar.gz | tar -xzf - --strip-components=1
RUN ./scripts/config.py set MBEDTLS_SSL_DTLS_SRTP
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS openssl

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/openssl/openssl/releases/download/openssl-3.2.1/openssl-3.2.1.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN /tmp/lib/Configure --prefix=/tmp/install ${OPENSSL_TARGET}
RUN make -j 4
RUN make install_sw


FROM base AS libcurl

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/curl/curl/archive/refs/tags/curl-8_6_0.tar.gz | tar -xzf - --strip-components=1

COPY --from=openssl /tmp/install /tmp/dependencies

WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install -DOPENSSL_ROOT_DIR=/tmp/dependencies -DOPENSSL_INCLUDE_DIR=/tmp/dependencies/include /tmp/lib
RUN make -j 4 install

FROM base AS libplog
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/SergiusTheBest/plog/archive/refs/tags/1.1.10.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS tinycbor
ARG TINYCBOR_VERSION=26c63e3d5977f77a6483edde4519489254670375
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/intel/tinycbor/archive/${TINYCBOR_VERSION}.tar.gz | tar -xzf - --strip-components=1
RUN make BUILD_SHARED=0 prefix=/tmp/install install

FROM base AS cjson
ARG CJSON_VERSION=v1.7.18
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/DaveGamble/cJSON/archive/${CJSON_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/dependencies -DBUILD_SHARED_LIBS=OFF /tmp/lib
RUN make -j 4 install

FROM base AS libevent
ARG LIBEVENT_VERSION=628f39bb908a032bc12ec5361723b1592022186e
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/libevent/libevent/archive/${LIBEVENT_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install -DBUILD_SHARED_LIBS=OFF /tmp/lib
RUN make -j 4 install


FROM base AS nabto_common
ARG NABTO_COMMON_VERSION=4d7329f62eec3e22d1870bcbdb960fb51d5c7ada
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/nabto/nabto-common/archive/${NABTO_COMMON_VERSION}.tar.gz | tar -xzf - --strip-components=1
COPY --from=mbedtls /tmp/install /tmp/dependencies
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DNABTO_DEVICE_PREFER_SYSTEM_LIBS=ON -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS nabto_embedded_sdk
ARG NABTO_EMBEDDED_SDK_VERSION=b05465f002a6b00722e14ba7955758fe919a5b08
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/nabto/nabto-embedded-sdk/archive/${NABTO_EMBEDDED_SDK_VERSION}.tar.gz | tar -xzf - --strip-components=1
COPY --from=mbedtls /tmp/install /tmp/dependencies
COPY --from=nabto_common /tmp/install /tmp/dependencies
COPY --from=tinycbor /tmp/install /tmp/dependencies
# the cjson install script has an error such that the install folder cannot be renamed.
COPY --from=cjson /tmp/dependencies /tmp/dependencies
COPY --from=libevent /tmp/install /tmp/dependencies
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DNABTO_DEVICE_PREFER_SYSTEM_LIBS=ON -DNABTO_DEVICE_BUILD_APPS=OFF -DNABTO_DEVICE_VERSION="0.0.0+${NABTO_EMBEDDED_SDK_VERSION}" -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_MODULE_PATH=/tmp/lib/cmake/vcpkg -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS libusrsctp
ARG LIBUSRSCTP_VERSION=0.9.5.0
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/sctplab/usrsctp/archive/${LIBUSRSCTP_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS libsrtp
ARG LIBSRTP_VERSION=v2.6.0
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/cisco/libsrtp/archive/${LIBSRTP_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS libjuice
ARG LIBJUICE_VERSION=1a6c2396aab2639636fbf12255a018396d6f6359
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/paullouisageneau/libjuice/archive/${LIBJUICE_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS nlohmann_json
ARG NLOHMANN_JSON_VERSION=v3.11.3
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/nlohmann/json/archive/${NLOHMANN_JSON_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DJSON_BuildTests=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install


FROM base AS libdatachannel
ARG LIBDATACHANNEL_VERSION=1691c95a57de60b7c277e0cdf8d53351218c2c7c
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/paullouisageneau/libdatachannel/archive/${LIBDATACHANNEL_VERSION}.tar.gz | tar -xzf - --strip-components=1
COPY --from=mbedtls /tmp/install /tmp/dependencies
COPY --from=tinycbor /tmp/install /tmp/dependencies
# the cjson install script has an error such that the install folder cannot be renamed.
COPY --from=cjson /tmp/dependencies /tmp/dependencies
COPY --from=libevent /tmp/install /tmp/dependencies
COPY --from=openssl /tmp/install /tmp/dependencies
COPY --from=libplog /tmp/install /tmp/dependencies
COPY --from=libusrsctp /tmp/install /tmp/dependencies
COPY --from=libsrtp /tmp/install /tmp/dependencies
COPY --from=libjuice /tmp/install /tmp/dependencies
COPY --from=nlohmann_json /tmp/install /tmp/dependencies
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DPREFER_SYSTEM_LIB=ON -DBUILD_SHARED_LIBS=OFF -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS cxxopts
ARG CXXOPTS_VERSION=v3.2.0
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/jarro2783/cxxopts/archive/${CXXOPTS_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install -DCXXOPTS_ENABLE_INSTALL=ON /tmp/lib
RUN make -j 4 install

FROM base AS jwtcpp
ARG JWT_CPP_VERSION=v0.7.0
COPY --from=openssl /tmp/install /tmp/dependencies
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/Thalhammer/jwt-cpp/archive/${JWT_CPP_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install



FROM base AS example

COPY --from=mbedtls /tmp/install /tmp/dependencies
COPY --from=openssl /tmp/install /tmp/dependencies
COPY --from=libcurl /tmp/install /tmp/dependencies
COPY --from=libplog /tmp/install /tmp/dependencies

COPY --from=nabto_embedded_sdk /tmp/install /tmp/dependencies
COPY --from=cjson /tmp/dependencies /tmp/dependencies
COPY --from=libevent /tmp/install /tmp/dependencies
COPY --from=tinycbor /tmp/install /tmp/dependencies
COPY --from=nabto_common /tmp/install /tmp/dependencies

COPY --from=libdatachannel /tmp/install /tmp/dependencies
COPY --from=libusrsctp /tmp/install /tmp/dependencies
COPY --from=libsrtp /tmp/install /tmp/dependencies
COPY --from=libjuice /tmp/install /tmp/dependencies
COPY --from=nlohmann_json /tmp/install /tmp/dependencies

COPY --from=cxxopts /tmp/install /tmp/dependencies
COPY --from=jwtcpp /tmp/install /tmp/dependencies

COPY ./src /tmp/lib/src
COPY ./CMakeLists.txt /tmp/lib/
COPY ./cmake /tmp/lib/cmake
COPY ./cmake-scripts /tmp/lib/cmake-scripts
COPY ./include /tmp/lib/include
COPY ./examples /tmp/lib/examples
COPY ./test /tmp/lib/test
COPY ./.git /tmp/lib/.git

#FROM base AS deadcode

WORKDIR /tmp/build
RUN cmake -DCMAKE_MODULE_PATH=/tmp/lib/cmake/vcpkg -DCMAKE_PREFIX_PATH=/tmp/dependencies -DNABTO_WEBRTC_BUILD_TESTS=OFF -DNABTO_WEBRTC_USE_VCPKG=OFF -DCMAKE_INSTALL_PREFIX=/tmp/install -DMbedTLS_ROOT=/tmp/dependencies -DCURL_ROOT=/tmp/dependencies -DOPENSSL_ROOT_DIR=/tmp/dependencies -Dplog_ROOT=/tmp/dependencies -DCMAKE_POLICY_DEFAULT_CMP0074=NEW -DCMAKE_POLICY_DEFAULT_CMP0135=NEW /tmp/lib
RUN make -j 4 install
RUN file /tmp/install/bin/edge_device_webrtc
#RUN $LDD /tmp/install/bin/edge_device_webrtc

#FROM debian:bookworm
#
#RUN dpkg --add-architecture arm64
#RUN apt-get update && apt-get install qemu-user qemu-user-static libc6:arm64 libstdc++-12-dev:arm64 -y
#
#COPY --from=example /tmp/install/bin/edge_device_webrtc /tmp/example/
#COPY --from=example /tmp/dependencies/lib/*.so* /tmp/example/

# Run it as LD_LIBRARY_PATH=/tmp/example qemu-aarch64-static ./edge_device_webrtc
