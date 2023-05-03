#!/bin/bash

CWD=`pwd`


SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )


INSTALL_DIR=${CWD}/install
MBEDTLS_BUILD_DIR=${CWD}/mbedtls-build
EXAMPLE_BUILD_DIR=${CWD}/example
MBEDTLS_SRC_DIR=${SCRIPT_DIR}/3rdparty/mbedtls

mkdir ${INSTALL_DIR}
mkdir ${MBEDTLS_BUILD_DIR}
mkdir ${EXAMPLE_BUILD_DIR}



cd ${MBEDTLS_BUILD_DIR}
cp ${SCRIPT_DIR}/mbedtls-config/mbedtls_config.h ${MBEDTLS_SRC_DIR}/include/mbedtls
cmake -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} ${MBEDTLS_SRC_DIR}
make -j 16
make install

cd ${EXAMPLE_BUILD_DIR}
cmake -DNABTO_DEVICE_MBEDTLS_PROVIDER=package -DMbedTLS_DIR=${INSTALL_DIR}/lib/cmake/MbedTLS -DPC_MbedTLS_LIBRARY_DIRS=${INSTALL_DIR}/lib -DPC_MbedTLS_INCLUDE_DIRS=${INSTALL_DIR}/include ${SCRIPT_DIR}
make -j 16
