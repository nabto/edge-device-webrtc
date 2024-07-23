# Ingenic T40 cross build

Firstly, obtain the toolchain and copy it to the root of this repo (Not this folder, this REPO). The toolchain should be named `mips-gcc720-glibc226.tar.gz`.

This integration builds a edge_device_webrtc demo which statically links openssl, curl and mbedtls

Then make the cross build, again from the root of this REPO, using:

```
docker build -f cross_build/ingenic-t40/Dockerfile -t edge_device_webrtc_t40 .
```

Copy the edge_device_webrtc executable from the built container image to the host:
```
docker run --rm --entrypoint cat edge_device_webrtc_t40 /tmp/install/bin/edge_device_webrtc > /tmp/edge_device_webrtc
```

Then the artifacts can be uploaded to s3 or a local webserver such that they can be transferred to the device.

```
aws s3 cp /tmp/edge_device_webrtc s3://downloads.nabto.com/assets/misc/edge_device_webrtc_<revision> --profile prod
```

It is recommended to pick a new name for the executable file for each upload to ensure the device does not get an earlier cached version.

on the device, the package can be downloaded using curl:

```
curl -o package.zip --insecure https://downloads.nabto.com/assets/misc/edge_device_webrtc_<revision>
```

The demo also needs a cacert.pem file that can be downloaded from https://curl.se/docs/caextract.html
