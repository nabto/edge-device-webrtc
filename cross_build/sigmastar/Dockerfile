FROM debian:bookworm as base

RUN apt-get update && apt-get install git build-essential cmake curl file tar -y
COPY gcc-sigmastar-9.1.0-2019.11-x86_64_arm-linux-gnueabihf.tar.gz /opt/
RUN tar xfz /opt/gcc-sigmastar-9.1.0-2019.11-x86_64_arm-linux-gnueabihf.tar.gz -C /opt/

ENV CC=/opt/gcc-sigmastar-9.1.0-2019.11-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-gcc
ENV CXX=/opt/gcc-sigmastar-9.1.0-2019.11-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf-g++
ARG OPENSSL_TARGET=linux-armv4

FROM base as mbedtls

RUN apt-get update && apt-get install python3 -y

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v3.5.2.tar.gz | tar -xzf - --strip-components=1
RUN ./scripts/config.py set MBEDTLS_SSL_DTLS_SRTP
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 16 install

FROM base as openssl

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/openssl/openssl/releases/download/openssl-3.2.1/openssl-3.2.1.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN /tmp/lib/Configure --prefix=/tmp/install ${OPENSSL_TARGET}
RUN make -j 16
RUN make install_sw


FROM base as libcurl

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/curl/curl/archive/refs/tags/curl-8_6_0.tar.gz | tar -xzf - --strip-components=1

COPY --from=openssl /tmp/install /tmp/dependencies

WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install -DOPENSSL_ROOT_DIR=/tmp/dependencies -DOPENSSL_INCLUDE_DIR=/tmp/dependencies/include /tmp/lib
RUN make -j 16 install

FROM base AS libplog

WORKDIR /tmp/lib
RUN curl -sSL https://github.com/SergiusTheBest/plog/archive/refs/tags/1.1.10.tar.gz | tar -xzf - --strip-components=1
WORKDIR /tmp/build
RUN cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/tmp/install /tmp/lib
RUN make -j 16 install

FROM base as example

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
RUN cmake -DCMAKE_BUILD_TYPE=Release -DNABTO_WEBRTC_BUILD_TESTS=OFF -DNABTO_WEBRTC_USE_VCPKG=OFF -DCMAKE_INSTALL_PREFIX=/tmp/install -DMbedTLS_ROOT=/tmp/dependencies -DCURL_ROOT=/tmp/dependencies -DOPENSSL_ROOT_DIR=/tmp/dependencies -Dplog_ROOT=/tmp/dependencies -DCMAKE_POLICY_DEFAULT_CMP0074=NEW -DCMAKE_POLICY_DEFAULT_CMP0135=NEW /tmp/lib
RUN make -j 16 install
RUN file /tmp/install/bin/edge_device_webrtc
#RUN $LDD /tmp/install/bin/edge_device_webrtc
