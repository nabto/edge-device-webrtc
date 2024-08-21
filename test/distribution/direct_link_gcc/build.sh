#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

SOURCE_DIR=${DIR}/../../..
BUILD_DIR=${SOURCE_DIR}/build/direct_link_gcc
INSTALL_DIR=${BUILD_DIR}/install

set -e
set -v

export CC=gcc
export CXX=g++

mkdir -p ${BUILD_DIR}

cd ${BUILD_DIR}
cmake -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${SOURCE_DIR}
make -j 16 install

cd ${BUILD_DIR}
g++ -I${INSTALL_DIR}/include -I${BUILD_DIR}/vcpkg_installed/x64-linux/include -L${INSTALL_DIR}/lib ${DIR}/test.cpp -lnabto_device_webrtc -lnabto_device -levent_extra -levent_pthreads -levent_core -lnn -lnabto_mdns -lnabto_stream -lnabto_stun -lnabto_coap -ltinycbor -lmbedtls -lmbedcrypto -lmbedx509

./a.out
