FROM debian:bookworm AS base

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install git build-essential cmake curl file tar bzip2 -y
COPY mips-gcc720-glibc226.tar.gz /opt/
RUN tar xf /opt/mips-gcc720-glibc226.tar.gz -C /opt/

ENV CC="/opt/mips-gcc720-glibc226/bin/mips-linux-gnu-gcc -muclibc"
ENV CXX="/opt/mips-gcc720-glibc226/bin/mips-linux-gnu-g++ -muclibc"
ENV CFLAGS="-march=mips32r2"
ENV CXXFLAGS="-D__STDC_FORMAT_MACROS"
ARG OPENSSL_TARGET=linux-mips32
ARG CMAKE_FIND_ROOT_PATH=/opt/mips-gcc720-glibc226/mips-linux-gnu/libc/uclibc

FROM base AS mbedtls

RUN apt-get update && apt-get install python3 -y

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/Mbed-TLS/mbedtls/releases/download/v3.6.0/mbedtls-3.6.0.tar.bz2 | tar -xjf - --strip-components=1
RUN ./scripts/config.py set MBEDTLS_SSL_DTLS_SRTP
WORKDIR /tmp/build
RUN cmake -DENABLE_PROGRAMS=OFF -DCMAKE_FIND_ROOT_PATH=${CMAKE_FIND_ROOT_PATH} -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 16 install || (echo "Make failed - dumping error logs and environment" >&2; env >&2; df -h >&2; cat CMakeFiles/CMakeError.log >&2; exit 1)

FROM base AS openssl

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/openssl/openssl/releases/download/openssl-3.2.1/openssl-3.2.1.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN /tmp/lib/Configure --prefix=/tmp/install ${OPENSSL_TARGET} no-async no-apps no-shared
RUN make -j 16
RUN make -j 16 install_sw

FROM base AS libcurl

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/curl/curl/archive/refs/tags/curl-8_6_0.tar.gz | tar -xzf - --strip-components=1

COPY --from=openssl /tmp/install /tmp/dependencies

WORKDIR /tmp/build
RUN cmake -DCMAKE_FIND_ROOT_PATH=${CMAKE_FIND_ROOT_PATH} -DBUILD_SHARED_LIBS=OFF -DBUILD_CURL_EXE=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install -DOPENSSL_ROOT_DIR=/tmp/dependencies -DOPENSSL_INCLUDE_DIR=/tmp/dependencies/include /tmp/lib
RUN make -j 16 install

FROM base AS libplog
WORKDIR /tmp/lib
RUN curl -sSL https://github.com/SergiusTheBest/plog/archive/refs/tags/1.1.10.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_FIND_ROOT_PATH=${CMAKE_FIND_ROOT_PATH} -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 16 install

FROM base AS example

COPY --from=mbedtls /tmp/install /tmp/dependencies
COPY --from=openssl /tmp/install /tmp/dependencies
COPY --from=libcurl /tmp/install /tmp/dependencies
COPY --from=libplog /tmp/install /tmp/dependencies

COPY ./src /tmp/lib/src
COPY ./CMakeLists.txt /tmp/lib/
COPY ./3rdparty /tmp/lib/3rdparty
COPY ./include /tmp/lib/include
COPY ./examples /tmp/lib/examples
COPY ./test /tmp/lib/test
COPY ./nabto-embedded-sdk /tmp/lib/nabto-embedded-sdk
COPY ./.git /tmp/lib/.git

WORKDIR /tmp/build
RUN cmake -DJWT_BUILD_EXAMPLES=OFF -DCMAKE_EXE_LINKER_FLAGS="-pthread -lm -latomic" -DCMAKE_FIND_ROOT_PATH=${CMAKE_FIND_ROOT_PATH} -DCMAKE_BUILD_TYPE=Release -DNO_WEBSOCKET=ON -DNABTO_WEBRTC_BUILD_TESTS=OFF -DNABTO_WEBRTC_USE_VCPKG=OFF -DCMAKE_INSTALL_PREFIX=/tmp/install -DMbedTLS_ROOT=/tmp/dependencies -DCURL_ROOT=/tmp/dependencies -DOPENSSL_ROOT_DIR=/tmp/dependencies -Dplog_ROOT=/tmp/dependencies -DCMAKE_POLICY_DEFAULT_CMP0074=NEW -DCMAKE_POLICY_DEFAULT_CMP0135=NEW /tmp/lib
RUN make -j 16 install VERBOSE=1
