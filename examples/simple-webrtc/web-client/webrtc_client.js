
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

  webrtcConnection.onClosed((error) => {
    if (error) {
      boxLog(`WebRTC connection closed: (${error.code}) ${error.message}`);
    } else {
      boxLog(`WebRTC connection closed`);
    }
    document.getElementById("disconnect").disabled = true;
    document.getElementById("connect").disabled = false;
  });

  document.getElementById("connect").disabled = true;

  try {
    await webrtcConnection.connect();
    document.getElementById("disconnect").disabled = false;
    boxLog("Connected to device. Getting video feed.")
    webrtcConnection.coapInvoke("GET","/webrtc/get");
  } catch (err) {
    boxLog(`Connect Failed: (${err.code}) ${err.message}`);
    document.getElementById("connect").disabled = false;
  }
}

async function disconnect()
{
  document.getElementById("disconnect").disabled = true;
  document.getElementById("connect").disabled = false;
  await webrtcConnection.close();
  webrtcConnection = null;

}

