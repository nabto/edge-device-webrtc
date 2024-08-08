
// import EdgeWebrtcConnectionFactory from "./edge-webrtc/edge_webrtc";

// var edgeWebrtc = require("./edge_webrtc_bundle")
//var edgeWebrtc = globalThis.window.NabtoEdgeWebrtcFactory;

var productId = null;
var deviceId = null;
var sct = null;

var webrtcConnection = null;
var connected = false;

async function connect()
{
  webrtcConnection = globalThis.window.EdgeWebrtc.createEdgeWebrtcConnection();
  productId = document.getElementById("productid").value;
  deviceId = document.getElementById("deviceid").value;
  sct = document.getElementById("sct").value;

  webrtcConnection.setConnectionOptions({productId: productId, deviceId: deviceId, sct: sct});

  webrtcConnection.onTrack((event, trackId) => {
  });

  await webrtcConnection.connect();
  connected = true;
}

async function upstreamAudio() {
  const constraints = window.constraints = {
    audio: true,
    video: false
  };
  let stream = await navigator.mediaDevices.getUserMedia(constraints);

  let auTrack = stream.getAudioTracks();

  if (auTrack.length > 0) {
    await webrtcConnection.addTrack(auTrack[0], "frontdoor-audio");
  }
}

async function upstreamVideo() {
  const constraints = window.constraints = {
    audio: false,
    video: true
  };
  let stream = await navigator.mediaDevices.getUserMedia(constraints);

  let track = stream.getVideoTracks();

  if (track.length > 0) {
    await webrtcConnection.addTrack(track[0], "frontdoor-video");
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
