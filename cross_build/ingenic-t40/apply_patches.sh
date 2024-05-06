#!/bin/bash

SCRIPT=$(readlink -f "$0")
SCRIPT_DIR=$(dirname "$SCRIPT")

set -e

cd $SCRIPT_DIR/../..
git apply --check cross_build/ingenic-t40/mips-gcc720-glibc226-std-round-compile-error.patch
git apply cross_build/ingenic-t40/mips-gcc720-glibc226-std-round-compile-error.patch

cd $SCRIPT_DIR/../../nabto-embedded-sdk
git apply --check ../cross_build/ingenic-t40/nabto-embedded-sdk-libm.patch
git apply ../cross_build/ingenic-t40/nabto-embedded-sdk-libm.patch
