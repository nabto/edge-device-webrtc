FROM debian:trixie AS base

RUN apt-get update && apt-get install git build-essential cmake gcc-aarch64-linux-gnu g++-aarch64-linux-gnu curl file tar unzip bzip2 -y

ENV CC=aarch64-linux-gnu-gcc
ENV CXX=aarch64-linux-gnu-g++
ARG OPENSSL_TARGET=linux-aarch64

FROM base AS mbedtls

RUN apt-get update && apt-get install python3 -y

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2 | tar -xjf - --strip-components=1
RUN ./scripts/config.py set MBEDTLS_SSL_DTLS_SRTP
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS openssl

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/openssl/openssl/releases/download/openssl-3.6.5/openssl-3.6.5.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN /tmp/lib/Configure --prefix=/tmp/install ${OPENSSL_TARGET}
RUN make -j 4
RUN make install_sw


FROM base AS libcurl

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/curl/curl/archive/refs/tags/curl-8_23_0.tar.gz | tar -xzf - --strip-components=1

COPY --from=openssl /tmp/install /tmp/dependencies

WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install -DOPENSSL_ROOT_DIR=/tmp/dependencies -DOPENSSL_INCLUDE_DIR=/tmp/dependencies/include -DCURL_USE_LIBPSL=OFF -DUSE_LIBIDN2=OFF /tmp/lib
RUN make -j 4 install

FROM base AS libplog
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/SergiusTheBest/plog/archive/refs/tags/1.1.11.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS tinycbor
ARG TINYCBOR_VERSION=26c63e3d5977f77a6483edde4519489254670375
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/intel/tinycbor/archive/${TINYCBOR_VERSION}.tar.gz | tar -xzf - --strip-components=1
RUN make BUILD_SHARED=0 prefix=/tmp/install install

FROM base AS cjson
ARG CJSON_VERSION=v1.7.19
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/DaveGamble/cJSON/archive/${CJSON_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/dependencies -DBUILD_SHARED_LIBS=OFF /tmp/lib
RUN make -j 4 install

FROM base AS libevent
# the libevent cmake config hardcodes the install prefix, so install directly into the final location.
ARG LIBEVENT_VERSION=release-2.1.13-stable
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/libevent/libevent/archive/${LIBEVENT_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/dependencies -DBUILD_SHARED_LIBS=OFF -DEVENT__LIBRARY_TYPE=STATIC -DEVENT__DISABLE_OPENSSL=ON -DEVENT__DISABLE_MBEDTLS=ON -DEVENT__DISABLE_BENCHMARK=ON -DEVENT__DISABLE_TESTS=ON -DEVENT__DISABLE_REGRESS=ON -DEVENT__DISABLE_SAMPLES=ON /tmp/lib
RUN make -j 4 install


FROM base AS nabto_common
ARG NABTO_COMMON_VERSION=a72ae1b05cdf71e96f2b4039eac10107d9772e9d
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/nabto/nabto-common/archive/${NABTO_COMMON_VERSION}.tar.gz | tar -xzf - --strip-components=1
COPY --from=mbedtls /tmp/install /tmp/dependencies
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DNABTO_DEVICE_PREFER_SYSTEM_LIBS=ON -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS nabto_embedded_sdk
ARG NABTO_EMBEDDED_SDK_VERSION=v5.15.1
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/nabto/nabto-embedded-sdk/archive/${NABTO_EMBEDDED_SDK_VERSION}.tar.gz | tar -xzf - --strip-components=1
COPY --from=mbedtls /tmp/install /tmp/dependencies
COPY --from=nabto_common /tmp/install /tmp/dependencies
COPY --from=tinycbor /tmp/install /tmp/dependencies
# the cjson install script has an error such that the install folder cannot be renamed.
COPY --from=cjson /tmp/dependencies /tmp/dependencies
COPY --from=libevent /tmp/dependencies /tmp/dependencies
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DNABTO_DEVICE_PREFER_SYSTEM_LIBS=ON -DNABTO_DEVICE_BUILD_APPS=OFF -DNABTO_DEVICE_VERSION="5.15.1" -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_MODULE_PATH=/tmp/lib/cmake/vcpkg -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS libusrsctp
ARG LIBUSRSCTP_VERSION=0.9.5.0
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/sctplab/usrsctp/archive/${LIBUSRSCTP_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS libsrtp
ARG LIBSRTP_VERSION=v2.8.1
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/cisco/libsrtp/archive/${LIBSRTP_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS libjuice
ARG LIBJUICE_VERSION=v1.7.4
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/paullouisageneau/libjuice/archive/${LIBJUICE_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS nlohmann_json
ARG NLOHMANN_JSON_VERSION=v3.12.0
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/nlohmann/json/archive/${NLOHMANN_JSON_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DJSON_BuildTests=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install


FROM base AS libdatachannel
# libdatachannel >= 0.24 honors BUILD_SHARED_LIBS, older versions always built the shared library.
ARG LIBDATACHANNEL_VERSION=v0.24.6
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/paullouisageneau/libdatachannel/archive/${LIBDATACHANNEL_VERSION}.tar.gz | tar -xzf - --strip-components=1
COPY --from=mbedtls /tmp/install /tmp/dependencies
COPY --from=tinycbor /tmp/install /tmp/dependencies
# the cjson install script has an error such that the install folder cannot be renamed.
COPY --from=cjson /tmp/dependencies /tmp/dependencies
COPY --from=libevent /tmp/dependencies /tmp/dependencies
COPY --from=openssl /tmp/install /tmp/dependencies
COPY --from=libplog /tmp/install /tmp/dependencies
COPY --from=libusrsctp /tmp/install /tmp/dependencies
COPY --from=libsrtp /tmp/install /tmp/dependencies
COPY --from=libjuice /tmp/install /tmp/dependencies
COPY --from=nlohmann_json /tmp/install /tmp/dependencies
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DPREFER_SYSTEM_LIB=ON -DBUILD_SHARED_LIBS=ON -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 4 install

FROM base AS cxxopts
ARG CXXOPTS_VERSION=v3.3.1
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/jarro2783/cxxopts/archive/${CXXOPTS_VERSION}.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_PREFIX_PATH=/tmp/dependencies -DCMAKE_INSTALL_PREFIX=/tmp/install -DCXXOPTS_ENABLE_INSTALL=ON /tmp/lib
RUN make -j 4 install

FROM base AS jwtcpp
ARG JWT_CPP_VERSION=v0.7.2
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
COPY --from=libevent /tmp/dependencies /tmp/dependencies
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

#FROM debian:trixie
#
#RUN dpkg --add-architecture arm64
#RUN apt-get update && apt-get install qemu-user qemu-user-static libc6:arm64 libstdc++-14-dev:arm64 -y
#
#COPY --from=example /tmp/install/bin/edge_device_webrtc /tmp/example/
#COPY --from=example /tmp/dependencies/lib/*.so* /tmp/example/

# Run it as LD_LIBRARY_PATH=/tmp/example qemu-aarch64-static ./edge_device_webrtc
