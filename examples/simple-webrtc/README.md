# Simple Webrtc Example

## Prepare the device

Before running this example, you must have created a device in the Nabto Cloud Console and configured it with a fingerprint. Running the example with the `--create-key` argument will create a Private Key and prints its value and its fingerprint:

```
./simple_webrtc_device --create-key
Created Raw private key:
  f1b70f3a72166b1455a16b5153dd6d8c729065217a853cc9c9847e9b08c88727
With fingerprint:
  1cf5fdb1d1e6c2d6be5aba35043f4a211002f608a1749bac12313c33480c52bc
```

The *Raw key* must be used when starting the example, the *Fingerprint* should be configured in the Nabto Cloud Console.

If you do not already have a device in the Nabto Cloud Console, follow our [general guide for embedded applications](https://docs.nabto.com/developer/platforms/embedded/applications.html#console).

## Prepare the client

This example includes a website used to connect to the device. Before using the client its dependencies must be installed. This requires [npm](https://github.com/npm/cli) to be installed on your system. Navigate to the `web-client` subdirectory of this example and install the dependencies:

```
cd web-client
npm install
```

## Usage

To start the device, use the device configuration created previously to start the device:

```
./simple_webrtc_device -p <PRODUCT_ID> -d <DEVICE_ID> -k <RAW_PRIVATE_KEY>
################################################################
# Device configuration string:
# productid:<PRODUCT_ID>,deviceId:<DEVICE_ID>,sct:demosct
################################################################
The device fingerprint: [1cf5fdb1d1e6c2d6be5aba35043f4a211002f608a1749bac12313c33480c52bc]
2024-02-23 10:56:41.312 ...sdk/src/core/nc_device.c(399)[_INFO] Starting Nabto Device
2024-02-23 10:56:41.628 ...k/src/core/nc_attacher.c(747)[_INFO] Device attached to basestation
```

Note the *Device configuration string*. This will be used to configure the client.

To start the client, open the `index.html` file from the `web-client` subdirectory of this example in a browser.

```
file:///PATH/TO/REPO/examples/simple-webrtc/web-client/index.html
```

Copy the *Device configuration string* into the website and press the *parse* button to fill in the device configuration.

Finally, press the *Connect* button to connect to the device.

