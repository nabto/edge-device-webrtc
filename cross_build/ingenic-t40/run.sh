#!/bin/sh

WEBRTC_LOG_LEVEL=trace
DEVICE_ID=de-xxxxxxxx
PRODUCT_ID=pr-xxxxxxxx
PRIVATE_KEY=32-byte-valid-ecc-private-key

RTSP_PATH=/second
RTSP_PORT=8554

SCRIPT=$(readlink -f "$0")
SCRIPT_DIR=$(dirname "$SCRIPT")

ROOT_DIR=$SCRIPT_DIR/..
CA_DIR=$ROOT_DIR/ca
LIB_DIR=$ROOT_DIR/lib
HOME_DIR=$ROOT_DIR/home

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$LIB_DIR

NABTO_WEBRTC_LOG_LEVEL=$WEBRTC_LOG_LEVEL $SCRIPT_DIR/edge_device_webrtc -k $PRIVATE_KEY  -d $DEVICE_ID -p $PRODUCT_ID --cacert $CA_DIR/cacert.pem  -H $HOME_DIR --rtsp rtsp://127.0.0.1:$RTSP_PORT/$RTSP_PATH

