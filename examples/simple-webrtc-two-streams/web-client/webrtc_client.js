
// import EdgeWebrtcConnectionFactory from "./edge-webrtc/edge_webrtc";

// var edgeWebrtc = require("./edge_webrtc_bundle")
//var edgeWebrtc = globalThis.window.NabtoEdgeWebrtcFactory;

var productId = null;
var deviceId = null;
var sct = null;

var webrtcConnection = null;
var connected = false;

function connect()
{
  webrtcConnection = globalThis.window.EdgeWebrtc.EdgeWebrtcConnectionFactory.create();
  productId = document.getElementById("product").value;
  deviceId = document.getElementById("device").value;
  sct = document.getElementById("sct").value;

  webrtcConnection.setConnectionOptions({productId: productId, deviceId: deviceId, sct: sct, signalingServerUrl: "wss://signaling.smartcloud.nabto.com"});

  webrtcConnection.onTrack((event, trackId) => {
    console.log(`onTrack event: ${JSON.stringify(event)}, trackId: ${trackId}`)
    const streams = event.streams;
    var videoElement;

    if (trackId === "feed1") {
      videoElement = document.getElementById("received_video1");
    } else if (trackId === "feed2") {
      videoElement = document.getElementById("received_video2");
    } else {
      console.log(`unknown track id ${trackId}`);
    }
    const track = event.transceiver.receiver.track;

    const stream = new MediaStream();
    stream.addTrack(track);
    videoElement.srcObject = stream;
  });

  webrtcConnection.onConnected(() => {
    connected = true;
  });

  webrtcConnection.connect();
}

function startFeed1() {
  if (connected) {
    webrtcConnection.coapInvoke("POST", "/webrtc/feed1");
  }
}

function startFeed2() {
  if (connected) {
    webrtcConnection.coapInvoke("POST", "/webrtc/feed2");
  }
}
