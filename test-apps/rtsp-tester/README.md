# RTSP tester

This will attempt to negotiate an RTSP session with a server given an URL eg.:

```
./test-apps/rtsp-tester/rtsp_client_tester rtsp://user:password@127.0.0.1:8554/video
```

When the RTSP client sends its DESCRIBE request, if the RTSP server responds with status 401, the client will look for the `WWW-Authenticate` response header. If the header is set to `Basic` or `Digest`, the client will use the username and password from the URL to authenticate to the server.

If you do not have an RTSP server, an example server is provided in `test-apps/rtsp-server/`.
