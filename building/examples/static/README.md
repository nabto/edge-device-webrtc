# This example shows how to build an executable for a linux system resulting on an executable where dependent libraries are statically linked.

Application compiled:
  A simple webrtc demo which can take input from an udp rtp source and serve it via webrtc. This demo does no

Target architecture:

The target architecture is arm

Libraries used:
  * static mbedtls library
  * nabto embedded sdk compiled using static libraries
  * libdatachannel using static libraries.

## How to build this container

in this directory run `docker build -f Dockerfile ../../../`
