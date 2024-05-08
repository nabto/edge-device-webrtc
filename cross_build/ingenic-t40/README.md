# Ingenic T40 cross build

Firstly, obtain the toolchain and copy it to the root of this repo (Not this folder, this REPO). The toolchain should be named `mips-gcc720-glibc226.tar.gz`.

Then, run the `./apply_patches.sh` script to fix some `std::round` errors.


Then make the cross build, again from the root of this REPO, using:

```
docker build -f cross_build/ingenic-t40/Dockerfile -t edge_device_webrtc_t40 .
```

The resulting artifacts are packaged into a zip file in the resulting container. You can upload this eg. to s3 to allow it to be downloaded on a device:

```
docker run -it -v ~/.aws:/root/.aws edge_device_webrtc_t40 aws s3 cp /tmp/package.zip s3://downloads.nabto.com/assets/misc/p2.zip --profile prod
```

It is recommended to pick a new name for the zip file (`p2.zip` above) for each upload to ensure the device does not get an earlier cached version.

on the device, the package can be downloaded using curl:

```
curl -o package.zip --insecure https://downloads.nabto.com/assets/misc/p2.zip
```
