# edge-device-webrtc
WebRTC Implementation for Nabto Embedded SDK



## Example usage

From the repo root:

```
mkdir build
cd build-scripts
docker compose up -d
docker compose exec device bash
cd build
cmake ../
make -j
./examples/webrtc-device/webrtc_device
```

in separate terminal:
```
cd test/signalling-server
npm install
ts-node app.ts
```

in separate terminal:
```
cd test/web-client
python3 -m http.server -d .
```

From a browser, go to `localhost:8000` press `log in`.

## TODO ideas

 * make docker container where libdatachannel and its dependencies are installed on the system and remove it as a git submodule here
 * use ephemeral stream port so we do not limit which ports costumers can use for their own streams
