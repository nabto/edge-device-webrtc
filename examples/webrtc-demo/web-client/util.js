

function setVideoReq() {
  let payload = {
    tracks: ["frontdoor-video"]
  }
  document.getElementById("coapmethod").value = "POST";
  document.getElementById("coappath").value = "/webrtc/tracks";
  document.getElementById("coapct").value = 50;
  document.getElementById("coappayload").value = JSON.stringify(payload);
  updateUi();
}

function setVideoAudioReq() {
  let payload = {
    tracks: ["frontdoor-video", "frontdoor-audio"]
  }
  document.getElementById("coapmethod").value = "POST";
  document.getElementById("coappath").value = "/webrtc/tracks";
  document.getElementById("coapct").value = 50;
  document.getElementById("coappayload").value = JSON.stringify(payload);
  updateUi();
}

function setDefaultTracksReq() {
  document.getElementById("coapmethod").value = "GET";
  document.getElementById("coappath").value = "/webrtc/tracks/default";
  document.getElementById("coapct").value = "";
  document.getElementById("coappayload").value = "";
  updateUi();
}

function setSignalingReq() {
  document.getElementById("coapmethod").value = "GET";
  document.getElementById("coappath").value = "/p2p/webrtc-info";
  document.getElementById("coapct").value = "";
  document.getElementById("coappayload").value = "";
  updateUi();
}

function setOauthReq() {
  document.getElementById("coapmethod").value = "POST";
  document.getElementById("coappath").value = "/webrtc/oauth";
  document.getElementById("coapct").value = 0;
  document.getElementById("coappayload").value = document.getElementById("oauthtoken").value;
  updateUi();
}

function boxLog(msg) {
  var listElem = document.querySelector(".logbox");

  var item = document.createElement("li");
  item.appendChild(document.createTextNode(msg));
  listElem.appendChild(item);
}

function clearLog() {
  var listElem = document.querySelector(".logbox");
  let first = listElem.firstChild;
  let next = undefined;
  while (next = first.nextSibling) {
    listElem.removeChild(next);
  }
  return;
}

function canCoap(isConnected) {
  console.log("can coap")
  if (!isConnected) {
    console.log("NOT CONNECTED")
    document.getElementById("coap").disabled = true;
    return;
  }
  let method = document.getElementById("coapmethod").value;
  if (method != "GET" &&
  method != "POST" &&
  method != "PUT" &&
  method != "DELETE"
  ) {
    document.getElementById("coap").disabled = true;
    return;
  }

  let path = document.getElementById("coappath").value;
  if (path.length == 0) {
    document.getElementById("coap").disabled = true;
    return;
  }


  document.getElementById("coap").disabled = false;
  return;

}
