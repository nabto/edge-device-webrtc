
let productId = null;
let deviceId = null;
let sct = null;
let webrtcConnection = null;
let datachannel = null;

async function connect()
{
  webrtcConnection = globalThis.window.EdgeWebrtc.createEdgeWebrtcConnection();
  productId = document.getElementById("product").value;
  deviceId = document.getElementById("device").value;
  sct = document.getElementById("sct").value;

  webrtcConnection.setConnectionOptions({productId: productId, deviceId: deviceId, sct: sct});

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
    document.getElementById("createDc").disabled = true;
    document.getElementById("dcsenddiv").hidden = true;
    document.getElementById("connect").disabled = false;
  });

  document.getElementById("connect").disabled = true;

  try {
    await webrtcConnection.connect();
    document.getElementById("disconnect").disabled = false;
    document.getElementById("createDc").disabled = false;
    boxLog("Connected to device. Getting video feed.")
    webrtcConnection.coapInvoke("POST","/webrtc/tracks");
  } catch (err) {
    boxLog(`Connect Failed: (${err.code}) ${err.message}`);
    document.getElementById("connect").disabled = false;
  }
}

async function disconnect()
{
  document.getElementById("disconnect").disabled = true;
  document.getElementById("createDc").disabled = true;
  document.getElementById("dcsenddiv").hidden = true;
  document.getElementById("connect").disabled = false;
  await webrtcConnection.close();
  webrtcConnection = null;

}

async function createDatachannel(label)
{
  // TODO: Use webrtcConnection.createDatachannel(label) once the js API is ready
  datachannel = webrtcConnection.pc.createDataChannel(label);
  datachannel.addEventListener("message", (event) => {
    console.log("Got datachannel message: ", event.data);
    let dec = new TextDecoder("utf-8");
    boxLog(`Got datachannel message: ${dec.decode(event.data)}`);
  });
  datachannel.addEventListener("open", (event) => {
    boxLog("Datachannel Opened!");
    document.getElementById("dcsenddiv").hidden = false;
  })
}

async function datachannelSend(data) {
  datachannel.send(data);
}
