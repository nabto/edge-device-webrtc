#!/bin/bash

CWD=`pwd`


SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )


INSTALL_DIR=${CWD}/install
MBEDTLS_BUILD_DIR=${CWD}/mbedtls-build
MBEDTLS_DIR=${CWD}/mbedtls
EXAMPLE_BUILD_DIR=${CWD}/example

mkdir ${INSTALL_DIR}
mkdir ${MBEDTLS_BUILD_DIR}
mkdir ${EXAMPLE_BUILD_DIR}
mkdir ${MBEDTLS_DIR}

cd ${MBEDTLS_DIR}
curl -sSL https://github.com/Mbed-TLS/mbedtls/archive/refs/tags/v3.4.0.tar.gz | tar -xzf - --strip-components=1

cd ${MBEDTLS_BUILD_DIR}
cp ${SCRIPT_DIR}/mbedtls-config/mbedtls_config.h ${MBEDTLS_DIR}/include/mbedtls
cmake -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} -DCMAKE_POSITION_INDEPENDENT_CODE=ON ${MBEDTLS_DIR}
make -j 16
make install

cd ${EXAMPLE_BUILD_DIR}
cmake -DENABLE_MBEDTLS=ON -DUSE_MBEDTLS=ON -DNABTO_DEVICE_MBEDTLS_PROVIDER=package -DMbedTLS_DIR=${INSTALL_DIR}/lib/cmake/MbedTLS -DPC_MbedTLS_LIBRARY_DIRS=${INSTALL_DIR}/lib -DPC_MbedTLS_INCLUDE_DIRS=${INSTALL_DIR}/include ${SCRIPT_DIR}
make -j 16
