
let productId = null;
let deviceId = null;
let sct = null;
let webrtcConnection = null;

async function connect()
{
  webrtcConnection = globalThis.window.EdgeWebrtc.createEdgeWebrtcConnection();
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

  document.getElementById("connect").disabled = true;
  await webrtcConnection.connect();
  document.getElementById("disconnect").disabled = false;
}

async function disconnect()
{
  document.getElementById("disconnect").disabled = true;
  document.getElementById("connect").disabled = false;
  await webrtcConnection.close();
  webrtcConnection = null;

}
