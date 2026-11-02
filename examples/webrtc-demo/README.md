# WebRTC demo device

The demo device application `edge_device_webrtc`. See the [top level README](../../README.md#running-the-example) for how to run it, and use the [desktop client](../desktop-client) to pair with it and view the feeds.


## CoAP endpoints


### POST /webrtc/tracks

Post list of track IDs the device should add to the WebRTC connection

request content format: `APPLICATION_JSON (50)`
request payload:
```
{
    tracks: string[]
}
```

response status: 201 on success

### GET /webrtc/tracks/default

List the IDs of the default video and audio tracks of the device.

response content format: `APPLICATION_JSON (50)`
response payload

```
{
    videoTrackId: string,
    audioTrackId: string
}
```
response status: 205 on success
