# WebRTC demo device



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

list the default tracks of the device. Used to construct metadata when backend creates an Alexa Offer.

response content format: `APPLICATION_JSON (50)`
response payload

```
{
    videoTrackId: string,
    audioTrackId: string
}
```
response status: 205 on success


### POST /webrtc/oauth

Authenticate connection using oAuth

request content format: `TEXT_PLAIN_UTF8 (0)`
request payload:
```
<OAUTH TOKEN STRING>
```

response status: 201 on success

### POST /webrtc/challenge

Challenge the device to prove it possesses its private key. Used from browsers to validate device identity to remove trust from the Signaling Service.

request content format: `APPLICATION_JSON (50)`
request payload:
```
{
    challenge: string
}
```
Request payload field `challenge` is a randomly generated nonce.


response content format: `APPLICATION_JSON (50)`
response payload

```
{
    response: string
}
```
Response Payload field `response` is a JWT token of the format:

```
{
    header: {
        alg: "ES256",
        jwk: {
            crv: "P-256",
            kty: "EC",
            x: string,
            y: string
        },
        typ: "JWS
    },
    payload: {
        client_fingerprint: string,
        device_fingerprint: string,
        nonce: string
    },
    signature: string
}
```

response status: 205 on success
