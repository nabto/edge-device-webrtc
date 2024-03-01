FROM debian:bookworm as base

RUN apt-get update && apt-get install git build-essential cmake gcc-aarch64-linux-gnu curl -y


FROM base as mbedtls

RUN apt-get update && apt-get install python3 -y

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v3.5.2.tar.gz | tar -xzf - --strip-components=1
RUN ./scripts/config.py set MBEDTLS_SSL_DTLS_SRTP
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 16 install

# FROM base as libsrtp

# COPY --from=mbedtls /tmp/install /tmp/dependencies

# WORKDIR /tmp/lib
# RUN curl -sSL https://github.com/cisco/libsrtp/archive/refs/tags/v2.6.0.tar.gz | tar -xzf - --strip-components=1
# RUN ./config.py set MBEDTLS_SSL_DTLS_SRTP
# WORKDIR /tmp/build
# RUN cmake -DCMAKE_INSTALL_PREFIX=/tmp/install -DENABLE_MBEDTLS=ON -DMbedTLS_ROOT=/tmp/dependencies /tmp/lib
# RUN make -j 16 install
# RUN ls -R  /tmp/install

# FROM base as libdatachannel

# WORKDIR /tmp
# RUN git clone --recursive https://github.com/paullouisageneau/libdatachannel
# COPY --from=mbedtls /tmp/install /tmp/dependencies

# # RUN curl -sSL https://github.com/paullouisageneau/libdatachannel/archive/refs/tags/v0.20.1.tar.gz | tar -xzf - --strip-components=1
# WORKDIR /tmp/build
# RUN cmake -DCMAKE_INSTALL_PREFIX=/tmp/install -DUSE_MBEDTLS=ON -DMbedTLS_ROOT=/tmp/dependencies -DCMAKE_POLICY_DEFAULT_CMP0074=NEW -DBUILD_SHARED_LIBS=OFF /tmp/libdatachannel
# RUN make -j 16 install

from base as libcurl

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/curl/curl/archive/refs/tags/curl-8_6_0.tar.gz | tar -xzf - --strip-components=1

COPY --from=mbedtls /tmp/install /tmp/dependencies

WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install -DCURL_USE_MBEDTLS=ON -DCURL_USE_OPENSSL=OFF -DMbedTLS_ROOT=/tmp/dependencies /tmp/lib
RUN make -j 16 install


FROM base as example

COPY --from=mbedtls /tmp/install /tmp/dependencies
#COPY --from=libdatachannel /tmp/install /tmp/dependencies
COPY --from=libcurl /tmp/install /tmp/dependencies

COPY ./src /tmp/lib/src
COPY ./CMakeLists.txt /tmp/lib/
COPY ./3rdparty /tmp/lib/3rdparty
COPY ./include /tmp/lib/include
COPY ./examples /tmp/lib/examples
COPY ./test /tmp/lib/test
COPY ./nabto-embedded-sdk /tmp/lib/nabto-embedded-sdk

WORKDIR /tmp/build
RUN cmake -DCMAKE_INSTALL_PREFIX=/tmp/install -DMbedTLS_ROOT=/tmp/dependencies -DCURL_ROOT=/tmp/dependencies -DCMAKE_POLICY_DEFAULT_CMP0074=NEW -DCMAKE_POLICY_DEFAULT_CMP0135=NEW /tmp/lib
RUN make -j 16 install

FROM debian:bookworm

COPY --from=example /tmp/install/bin /tmp/example
RUN ls -lah /tmp/example
