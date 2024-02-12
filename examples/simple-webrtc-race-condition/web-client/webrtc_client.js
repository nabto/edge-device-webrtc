
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
  webrtcConnection = globalThis.window.EdgeWebrtc.createEdgeWebrtcConnection();
  productId = document.getElementById("productid").value;
  deviceId = document.getElementById("deviceid").value;
  sct = document.getElementById("sct").value;

  webrtcConnection.setConnectionOptions({productId: productId, deviceId: deviceId, sct: sct, signalingServerUrl: "wss://signaling.smartcloud.nabto.com"});

  webrtcConnection.onTrack((event, trackId) => {
    console.log(`onTrack event: ${JSON.stringify(event)}, trackId: ${trackId}`)
    const streams = event.streams;
    var videoElement = document.getElementById("received_video1");

    if (trackId === "from_device") {
      var videoElement = document.getElementById("from_device");
      const track = event.transceiver.receiver.track;

      const stream = new MediaStream();
      stream.addTrack(track);
      videoElement.srcObject = stream;
    }
  });

  webrtcConnection.onConnected(() => {
    connected = true;
  });

  webrtcConnection.connect();
}

async function sendVideo() {
  const constraints = window.constraints = {
    audio: false,
    video: true
  };
  let stream = await navigator.mediaDevices.getUserMedia(constraints);
  let track = stream.getVideoTracks();

  if (track.length > 0) {
    await webrtcConnection.addTrack(track[0], "from_browser");
  }
}

async function receiveVideo() {
  webrtcConnection.coapInvoke("POST", "/webrtc/from_device");
}

async function createRaceCondition() {
  const constraints = window.constraints = {
    audio: false,
    video: true
  };
  let stream = await navigator.mediaDevices.getUserMedia(constraints);

  // now we have permissions,
  // aks the device for video before adding a track in this end.
  webrtcConnection.coapInvoke("POST", "/webrtc/from_device");

  let track = stream.getVideoTracks();

  if (track.length > 0) {
    await webrtcConnection.addTrack(track[0], "from_browser");
  }
}

addEventListener("load", (event) => {
  updateInfo();
});

function storeInfo() {
  productId = document.getElementById("productid").value;
  deviceId = document.getElementById("deviceid").value;
  sct = document.getElementById("sct").value;

  localStorage.setItem("productId", productId);
  localStorage.setItem("deviceId", deviceId);
  localStorage.setItem("sct", sct);
  updateInfo();
}

function updateInfo() {
  const prod = localStorage.getItem("productId");
  if (prod) {
    document.getElementById("productid").value = prod;
    productId = prod;
  }
  const dev = localStorage.getItem("deviceId");
  if (dev) {
    document.getElementById("deviceid").value = dev;
    deviceId = dev;
  }
  const sctLoc = localStorage.getItem("sct");
  if (sctLoc) {
    document.getElementById("sct").value = sctLoc;
    sct = sctLoc;
  }
};
