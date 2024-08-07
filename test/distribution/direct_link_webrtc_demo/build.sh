#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

SOURCE_DIR=${DIR}/../../..
BUILD_DIR=${SOURCE_DIR}/build/direct_link_webrtc_demo
INSTALL_DIR=${BUILD_DIR}/install
WEBRTC_DEMO_SOURCE_DIR=${SOURCE_DIR}/examples/webrtc-demo

set -e
set -v

export CC=gcc
export CXX=g++

mkdir -p ${BUILD_DIR}

cd ${BUILD_DIR}
cmake -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${SOURCE_DIR}
make -j 16 install

cd ${BUILD_DIR}
g++ -I${INSTALL_DIR}/include -I${BUILD_DIR}/vcpkg_installed/x64-linux/include -L${INSTALL_DIR}/lib -L${BUILD_DIR}/vcpkg_installed/x64-linux/lib \
  ${WEBRTC_DEMO_SOURCE_DIR}/main.cpp \
  ${WEBRTC_DEMO_SOURCE_DIR}/iam_util.cpp \
  ${WEBRTC_DEMO_SOURCE_DIR}/nabto_device.cpp \
  -levent_queue_impl -lrtsp_client -lrtp_client -lrtp_repacketizers -lrtp_packetizers -lfifo_file_client -ltrack_negotiators -lnabto_device_webrtc -ldatachannel-static -ljuice -lusrsctp -lsrtp2 -lnm_iam -lnabto_device -levent_extra -levent_pthreads -levent_core -lnn -lnabto_mdns -lnabto_stream -lnabto_stun -lnabto_coap -ltinycbor -lmbedtls -lmbedcrypto -lmbedx509 -lcjson -lwebrtc_util -lcurl -lz -lssl -lcrypto

./a.out
