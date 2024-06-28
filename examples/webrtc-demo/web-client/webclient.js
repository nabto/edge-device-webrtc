const CoapContentFormat = {
  TEXT_PLAIN_UTF8: 0,
  APPLICATION_LINK_FORMAT: 40,
  XML: 41,
  APPLICATION_OCTET_STREAM: 42,
  APPLICATION_JSON: 50,
  APPLICATION_CBOR: 60
};

let productId = null;
let deviceId = null;
let sct = null;
let fingerprint = null;
let username = null;
let password = null;
let webrtcConnection = null;
let connected = false;

function connect() {
  if (connected) {
    return hangUpCall();
  }
  boxLog("Connecting to device: ");
  boxLog(`* Product ID: ${productId}`);
  boxLog(`* Device ID: ${deviceId}`);
  boxLog(`* sct: ${sct}`);
  boxLog(`* fingerprint: ${fingerprint}`);
  boxLog(`* username: ${username}`);
  boxLog(`* password: ${password}`);

  webrtcConnection = globalThis.window.EdgeWebrtc.createEdgeWebrtcConnection();

  // TODO: Remove signalingUrl once an official non-demo signaling service is deployed
  let sigUrl = "wss://signaling.smartcloud.nabto.com";
  webrtcConnection.setConnectionOptions({deviceId: deviceId, productId: productId, sct: sct, signalingServerUrl: sigUrl});

  webrtcConnection.onClosed((error) => {
    if (error) {
      boxLog(`Connection Closed!: (${error.code}) ${error.message}`);
    } else {
      boxLog(`Connection Closed!`);
    }
    connected = false;
    updateUi();
  });

  webrtcConnection.onTrack(async (event, trackId, error) => {
    boxLog(`Received track event for ${trackId}`);
    if (error) {
      boxLog(`Track failed with error: ${error}`);
      return;
    }
    var stream = event.streams[0];
    if (event.track.kind != "video") {
      console.log(`${trackId} not video track, must be audio`);
      var audio = document.getElementById("received_audio");
      if ("srcObject" in audio) {
        try {
          audio.srcObject = stream;
        } catch(err) {
          console.log("Caught error: ", err);
          audio.src = URL.createObjectURL(stream);
        }
      } else {
        audio.src = URL.createObjectURL(stream);
      }
      return;
    }

    var video = document.getElementById("received_video");
    if ("srcObject" in video) {
      try {
        video.srcObject = stream;
      } catch(err) {
        console.log("Caught error: ", err);
        video.src = URL.createObjectURL(stream);
      }
    } else {
      video.src = URL.createObjectURL(stream);
    }

  });

  webrtcConnection.connect().then(()=> {
    boxLog("Connected to device!")
    connected = true;
    updateUi();
  }).catch((err) => {
    boxLog(`Connection failed with: ${err}`);
  });


}

async function coapInvoke() {
  let method = document.getElementById("coapmethod").value;
  let path = document.getElementById("coappath").value;
  let reqCf = undefined;
  let payload = document.getElementById("coappayload").value;
  if (payload && payload.length > 0) {
    reqCf = parseInt(document.getElementById("coapct").value);
  } else {
    payload = undefined;
  }

  let resp = await webrtcConnection.coapInvoke(method, path, reqCf, payload);
  console.log("resp: ", resp);
  if (resp.statusCode > 299) {
    boxLog(`CoAP Request failed!`);
  } else {
    boxLog("CoAP Request Succeeded!")
  }
  boxLog(`* response code: ${resp.statusCode}`);
  let cf = resp.contentFormat;
  if (cf == CoapContentFormat.TEXT_PLAIN_UTF8) {
    let data = String.fromCharCode.apply(String, resp.payload);
    boxLog(`* response UTF-8 payload: ${data}`);
  } else if (cf && cf == CoapContentFormat.APPLICATION_JSON) {
    let data = String.fromCharCode.apply(String, resp.payload);
    let json = JSON.parse(data);
    boxLog(`* response JSON payload: ${JSON.stringify(json)}`);
  } else if (cf && cf == CoapContentFormat.APPLICATION_CBOR) {
    let data = globalThis.window.nabtoModuleUtil.decodeCborPayload(resp.payload);
    boxLog(`* response CBOR payload: ${JSON.stringify(data)}`);
  } else if (resp.payload){
    boxLog(`* response Content Format: ${resp.contentFormat}`);
    boxLog(`* response payload: ${JSON.stringify(resp.payload)}`);
  } else {
    boxLog(`* response without: ${JSON.stringify(resp)}`);
  }
}


async function hangUpCall() {
  await webrtcConnection.close();
}

async function passAuth() {
  webrtcConnection.passwordAuthenticate(username, password).then(() => {
    boxLog("Successfully authenticated using password");

  }).catch((err) => {
    boxLog("Failed to authenticate using password");
  });
}

async function validateFingerprint() {
  webrtcConnection.validateFingerprint(fingerprint).then((valid) => {
    if (valid) {
      boxLog("Fingerprint Validated!");
    } else {
      boxLog("Fingerprint validation Failed!");
    }

  }).catch((err) => {
    boxLog("Fingerprint validation Failed!");
  });

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

function parseLink() {
  const devLink = document.getElementById("devicelink").value;
  if (devLink.length == 0) {
    boxLog("Invalid device link!");
    return false;
  }

  let params = devLink.split('?')[1].split('&');
  for (let p of params) {
    let [key, value] = p.split('=');
    if (key == "p") {
      productId = value;
      localStorage.setItem("productId", productId);
    } else if (key == "d") {
      deviceId = value;
      localStorage.setItem("deviceId", deviceId);
    } else if (key == "u") {
      username = value;
      localStorage.setItem("username", username);
    } else if (key == "pwd") {
      password = value;
      localStorage.setItem("password", password);
    } else if (key == "sct") {
      sct = value;
      localStorage.setItem("sct", sct);
    } else if (key == "fp") {
      fingerprint = value;
      localStorage.setItem("fingerprint", fingerprint);
    }
  }
  updateInfo();
  updateUi();
  return true;
}

addEventListener("load", (event) => {
  updateInfo();
});

function storeInfo() {
  productId = document.getElementById("product").value;
  deviceId = document.getElementById("device").value;
  username = document.getElementById("iamuser").value;
  password = document.getElementById("iampwd").value;
  sct = document.getElementById("sct").value;
  fingerprint = document.getElementById("fp").value;

  localStorage.setItem("productId", productId);
  localStorage.setItem("deviceId", deviceId);
  localStorage.setItem("username", username);
  localStorage.setItem("password", password);
  localStorage.setItem("sct", sct);
  localStorage.setItem("fingerprint", fingerprint);
  updateInfo();
  updateUi();
}

function updateInfo() {
  const prod = localStorage.getItem("productId");
  if (prod) {
    document.getElementById("product").value = prod;
    productId = prod;
  }
  const dev = localStorage.getItem("deviceId");
  if (dev) {
    document.getElementById("device").value = dev;
    deviceId = dev;
  }
  const uname = localStorage.getItem("username");
  if (uname) {
    document.getElementById("iamuser").value = uname;
    username = uname;
  }
  const pwd = localStorage.getItem("password");
  if (pwd) {
    document.getElementById("iampwd").value = pwd;
    password = pwd;
  }
  const sctLoc = localStorage.getItem("sct");
  if (sctLoc) {
    document.getElementById("sct").value = sctLoc;
    sct = sctLoc;
  }
  const fp = localStorage.getItem("fingerprint");
  if (fp) {
    document.getElementById("fp").value = fp;
    fingerprint = fp;
  }

  const link = localStorage.getItem("deviceLink");
  if (link) {
    document.getElementById("devicelink").value = link;
  }
};


function updateUi() {
  document.getElementById("login").value = connected ? "Disconnect" : "Connect";
  document.getElementById("passauth").disabled = (!connected || !username || !password);
  document.getElementById("fpvalid").disabled = (!connected || !fingerprint);
  document.getElementById("oauth").disabled = document.getElementById("oauthtoken").value.length == 0;
  document.getElementById("audioup").disabled = !connected;
  document.getElementById("videoup").disabled = !connected;
  canCoap(connected);
}
