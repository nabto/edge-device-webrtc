// WebSocket and WebRTC based multi-user chat sample with two-way video
// calling, including use of TURN if applicable or necessary.
//
// This file contains the JavaScript code that implements the client-side
// features for connecting and managing chat and video calls.
//
// To read about how this sample works:  http://bit.ly/webrtc-from-chat
//
// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/publicdomain/zero/1.0/


var nabtoSignaling = new NabtoWebrtcSignaling();
var nabtoConnection = new NabtoWebrtcConnection();

var productId = null;
var deviceId = null;
var sct = null;
var myPeerConnection = null;    // RTCPeerConnection
var transceiver = null;         // RTCRtpTransceiver
var webcamStream = null;        // MediaStream from webcam

var iceServers = [{ urls: "stun:stun.nabto.net" }]; // Servers to use for Turn/stun

// Output logging information to console.

function log(text) {
  var time = new Date();

  console.log("[" + time.toLocaleTimeString() + "] " + text);
}

// Output an error message to console.

function log_error(text) {
  var time = new Date();

  console.trace("[" + time.toLocaleTimeString() + "] " + text);
}

function boxLog(msg) {
  var listElem = document.querySelector(".logbox");

  var item = document.createElement("li");
  item.appendChild(document.createTextNode(msg));
  listElem.appendChild(item);
}

function reset() {
  myPeerConnection = null;    // RTCPeerConnection
  transceiver = null;         // RTCRtpTransceiver
  webcamStream = null;        // MediaStream from webcam

}

// Open and configure the connection to the WebSocket server.

function connect() {
  reset();

  log(`Connecting to server: ${nabtoSignaling.signalingHost}`);
  boxLog(`Connecting to server: ${nabtoSignaling.signalingHost}`);

  productId = document.getElementById("product").value;
  deviceId = document.getElementById("device").value;
  sct = document.getElementById("sct").value;

  nabtoSignaling.setDeviceConfigSct(productId, deviceId, sct);

  nabtoSignaling.onconnected = (msg) => {
    nabtoSignaling.requestTurnCredentials();
  };

  nabtoSignaling.onoffer = (msg) => {
    handleVideoOfferMsg(msg);
  };

  nabtoSignaling.onanswer = (msg) => {
    handleVideoAnswerMsg(msg);
  };

  nabtoSignaling.onicecandidate = (msg) => {
    handleNewICECandidateMsg(msg);
  };

  nabtoSignaling.onturncredentials = (msg) => {
    handleTurnResponse(msg);
  };

  nabtoSignaling.signalingConnect();
}


function createPeerConnection() {
  log("Setting up a WebRTC connection...");

  // Create an RTCPeerConnection which knows to use our chosen
  // STUN server.

  myPeerConnection = new RTCPeerConnection({
    iceServers: iceServers,
    // iceTransportPolicy: "relay",
  });

  // Set up event handlers for the ICE negotiation process.

  myPeerConnection.onicecandidate = handleICECandidateEvent;
  myPeerConnection.oniceconnectionstatechange = handleICEConnectionStateChangeEvent;
  myPeerConnection.onicegatheringstatechange = handleICEGatheringStateChangeEvent;
  myPeerConnection.onsignalingstatechange = handleSignalingStateChangeEvent;
  myPeerConnection.onnegotiationneeded = handleNegotiationNeededEvent;
  myPeerConnection.ontrack = handleTrackEvent;
}

// Called by the WebRTC layer to let us know when it's time to
// begin, resume, or restart ICE negotiation.

async function handleNegotiationNeededEvent() {
  log("*** Negotiation needed");
  // TODO: renegotiation
}

// Called by the WebRTC layer when events occur on the media tracks
// on our WebRTC call. This includes when streams are added to and
// removed from the call.
//
// track events include the following fields:
//
// RTCRtpReceiver       receiver
// MediaStreamTrack     track
// MediaStream[]        streams
// RTCRtpTransceiver    transceiver
//
// In our case, we're just taking the first stream found and attaching
// it to the <video> element for incoming media.

async function handleTrackEvent(event) {
  log("*** Track event. streams length: " + event.streams.length);
  var stream = event.streams[0];
  console.log("   Track event: ", event);
  console.log("  event Stream: ", stream);
  console.log("  event Stream.tracks: ", stream.getTracks());
  if (event.track.kind != "video") {
    console.log("Not video track, ignoring");
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
}

// Handles |icecandidate| events by forwarding the specified
// ICE candidate (created by our local ICE agent) to the other
// peer through the signaling server.
function handleICECandidateEvent(event) {
  if (event.candidate) {
    log("*** Outgoing ICE candidate: " + event.candidate.candidate);
    nabtoSignaling.sendIceCandidate(event.candidate);
  }
}

// Handle |iceconnectionstatechange| events. This will detect
// when the ICE connection is closed, failed, or disconnected.
//
// This is called when the state of the ICE agent changes.

function handleICEConnectionStateChangeEvent(event) {
  log("*** ICE connection state changed to " + myPeerConnection.iceConnectionState);

  switch (myPeerConnection.iceConnectionState) {
    case "closed":
    case "failed":
    case "disconnected":
      closeVideoCall();
      break;
  }
}

// Set up a |signalingstatechange| event handler. This will detect when
// the signaling connection is closed.
//
// NOTE: This will actually move to the new RTCPeerConnectionState enum
// returned in the property RTCPeerConnection.connectionState when
// browsers catch up with the latest version of the specification!

async function handleSignalingStateChangeEvent(event) {
  log("*** WebRTC signaling state changed to: " + myPeerConnection.signalingState);
  switch (myPeerConnection.signalingState) {
    case "closed":
      closeVideoCall();
      break;
  }
}

// Handle the |icegatheringstatechange| event. This lets us know what the
// ICE engine is currently working on: "new" means no networking has happened
// yet, "gathering" means the ICE engine is currently gathering candidates,
// and "complete" means gathering is complete. Note that the engine can
// alternate between "gathering" and "complete" repeatedly as needs and
// circumstances change.
//
// We don't need to do anything when this happens, but we log it to the
// console so you can see what's going on when playing with the sample.

function handleICEGatheringStateChangeEvent(event) {
  log("*** ICE gathering state changed to: " + myPeerConnection.iceGatheringState);
}


async function handleTurnResponse(msg) {
  boxLog(`Received TURN Server Configuration:`);
  boxLog("[");
  for (let s of msg.servers) {
    // TODO: add port somewhere
    iceServers.push({
      urls: `${s.hostname}`,
      username: s.username,
      credential: s.password,
    });
    boxLog(`{`);
    boxLog(`  hostname: ${s.hostname},`);
    boxLog(`  port: ${s.port},`);
    boxLog(`  username: ${s.username},`);
    boxLog(`  password: ${s.password},`);
    boxLog(`},`);
  }
  boxLog("]");

  createPeerConnection();
  // const offer = await myPeerConnection.createOffer({offerToReceiveAudio: true, offerToReceiveVideo: true});
  // const offer = await myPeerConnection.createOffer({ offerToReceiveVideo: true});
  let coapChannel = myPeerConnection.createDataChannel("coap");

  coapChannel.addEventListener("open", (event) => {
    console.log("Datachannel Opened");
    boxLog("Datachannel Opened");
    nabtoConnection.setCoapDataChannel(coapChannel);

    nabtoConnection.coapInvoke("GET", "/p2p/endpoints", undefined, undefined, (resp) => {
      boxLog(`Coap response recieved: ${resp}`);
    });
  });

  myPeerConnection.addTransceiver("video", {direction: "recvonly", streams: []});
  const offer = await myPeerConnection.createOffer();

  await myPeerConnection.setLocalDescription(offer);

  nabtoSignaling.sendOffer(myPeerConnection.localDescription);
}

// Close the RTCPeerConnection and reset variables so that the user can
// make or receive another call if they wish. This is called both
// when the user hangs up, the other user hangs up, or if a connection
// failure is detected.

function closeVideoCall() {
  log("Closing the call");

  // Close the RTCPeerConnection

  if (myPeerConnection) {
    log("--> Closing the peer connection");

    // Disconnect all our event listeners; we don't want stray events
    // to interfere with the hangup while it's ongoing.

    myPeerConnection.ontrack = null;
    myPeerConnection.onnicecandidate = null;
    myPeerConnection.oniceconnectionstatechange = null;
    myPeerConnection.onsignalingstatechange = null;
    myPeerConnection.onicegatheringstatechange = null;
    myPeerConnection.onnotificationneeded = null;

    // Stop all transceivers on the connection

    myPeerConnection.getTransceivers().forEach(transceiver => {
      transceiver.stop();
    });

    // Close the peer connection

    myPeerConnection.close();
    myPeerConnection = null;
    nabtoConnection = null;
  }

}

// Hang up the call by closing our end of the connection, then
// sending a "hang-up" message to the other peer (keep in mind that
// the signaling is done on a different connection). This notifies
// the other peer that the connection should be terminated and the UI
// returned to the "no call in progress" state.

function hangUpCall() {
  if (myPeerConnection) {
    closeVideoCall();
  }
}

// Accept an offer to video chat. We configure our local settings,
// create our RTCPeerConnection, get and attach our local camera
// stream, then create and send an answer to the caller.

async function handleVideoOfferMsg(msg) {
  // If we're not already connected, create an RTCPeerConnection
  // to be linked to the caller.

  boxLog(`Recieved WebRTC offer from device! Sending answer`);
  console.log("Received video chat offer With SDP: ", msg.data);
  if (!myPeerConnection) {
    createPeerConnection();
  }

  var desc = new RTCSessionDescription(msg.data);

  log("  - Setting remote description");
  await myPeerConnection.setRemoteDescription(desc);
  await myPeerConnection.setLocalDescription(await myPeerConnection.createAnswer());

  nabtoSignaling.sendAnswer(myPeerConnection.localDescription);
}

// Responds to the "video-answer" message sent to the caller
// once the callee has decided to accept our request to talk.

async function handleVideoAnswerMsg(msg) {
  log("*** Call recipient has accepted our call");

  // Configure the remote description, which is the SDP payload
  // in our "video-answer" message.

  boxLog(`Recieved WebRTC answer from device!`);
  var desc = new RTCSessionDescription(msg.data);
  await myPeerConnection.setRemoteDescription(desc).catch(reportError);
}

// A new ICE candidate has been received from the other peer. Call
// RTCPeerConnection.addIceCandidate() to send it along to the
// local ICE framework.

async function handleNewICECandidateMsg(msg) {

  log("*** Adding received ICE candidate: " + msg.data);
  try {
    var candidate = new RTCIceCandidate(msg.data);
    await myPeerConnection.addIceCandidate(candidate)
  } catch (err) {
    reportError(err);
  }
}

// Handles reporting errors. Currently, we just dump stuff to console but
// in a real-world application, an appropriate (and user-friendly)
// error message should be displayed.

function reportError(errMessage) {
  log_error(`Error ${errMessage.name}: ${errMessage.message}`);
}
