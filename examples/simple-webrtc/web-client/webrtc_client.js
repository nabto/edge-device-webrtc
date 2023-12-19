
// import EdgeWebrtcConnectionFactory from "./edge-webrtc/edge_webrtc";

// var edgeWebrtc = require("./edge_webrtc_bundle")
var edgeWebrtc = window.NabtoEdgeWebrtcFactory;

var productId = null;
var deviceId = null;
var sct = null;

function connect()
{
  let webrtcConnection = edgeWebrtc.create();
  productId = document.getElementById("product").value;
  deviceId = document.getElementById("device").value;
  sct = document.getElementById("sct").value;

  webrtcConnection.setConnectionOptions({productId: productId, deviceId: deviceId, sct: sct, signalingServerUrl: "wss://signaling.smartcloud.nabto.com"});

  webrtcConnection.onTrack((event, trackId) => {
    var video = document.getElementById("received_video");
    video.srcObject = event.streams[0];
  });

  webrtcConnection.onConnected(() => {
    webrtcConnection.coapInvoke("GET","/webrtc/get");

  });

  webrtcConnection.connect();
}
