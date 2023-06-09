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

"use strict";

// Get our hostname

var myHostname = "localhost";

// WebSocket chat/signaling channel variables.
var connection = null;

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

// Send a JavaScript object by converting it to JSON and sending
// it as a message on the WebSocket connection.

function sendToServer(msg) {
  var msgJSON = JSON.stringify(msg);

  log("Sending '" + msg.type + "' message: " + msgJSON);
  connection.send(msgJSON);
}


function reset() {
  myPeerConnection = null;    // RTCPeerConnection
  transceiver = null;         // RTCRtpTransceiver
  webcamStream = null;        // MediaStream from webcam

}

// Open and configure the connection to the WebSocket server.

function connect() {
  reset();
  var serverUrl;
  var scheme = "ws";

  serverUrl = scheme + "://" + myHostname + ":6503";

  log(`Connecting to server: ${serverUrl}`);
  boxLog(`Connecting to server: ${serverUrl}`);
  connection = new WebSocket(serverUrl, "json");

  connection.onerror = function (evt) {
    console.dir(evt);
  }

  connection.onmessage = function (evt) {
    console.log("Message received: ", evt.data);
    var msg = JSON.parse(evt.data);

    switch (msg.type) {
      case 21: // WEBSOCK_LOGIN_RESPONSE
        handleLoginResponse(msg);
        break;
      case 0:  // video offer (should not be needed)
        handleVideoOfferMsg(msg);
        break;

      case 1:  // video answer from device
        handleVideoAnswerMsg(msg);
        break;

      case 2: // A new ICE candidate has been received
        handleNewICECandidateMsg(msg);
        break;

      case 4: // TURN response
        handleTurnResponse(msg);
        break;

      default:
        log_error("Unknown message received:");
        log_error(msg);
    }

  };

  productId = document.getElementById("product").value;
  deviceId = document.getElementById("device").value;
  sct = document.getElementById("sct").value;

  connection.onopen = function (evt) {
    let loginReq = {
      type: 20,
      productId: productId,
      deviceId: deviceId,
      sct: sct,
      username: "foobar",
      password: "foobar",
    }

    sendToServer(loginReq);
  }

}

// Create the RTCPeerConnection which knows how to talk to our
// selected STUN/TURN server and then uses getUserMedia() to find
// our camera and microphone and add that stream to the connection for
// use in our video call. Then we configure event handlers to get
// needed notifications on the call.

function createPeerConnection() {
  log("Setting up a connection...");

  // Create an RTCPeerConnection which knows to use our chosen
  // STUN server.

  myPeerConnection = new RTCPeerConnection({
    iceServers: iceServers,
    // [     // Information about ICE servers - Use your own!
    //   // {
    //   //   urls: "turn:" + myHostname,  // A TURN server
    //   //   username: "1675854660:foo",
    //   //   credential: "gTDTZxG+R4BCJRk0NEkkB1DUYyw="
    //   // },
    //   // {
    //   //   urls: "turn:34.245.62.208",  // A TURN server
    //   //   username: "1675935678:foo",
    //   //   credential: "D/9Nw9yGzXGL+qy/mvwLlXfOgVI="
    //   // },
    //   {
    //     urls: "stun:stun.nabto.net",
    //   },
    // ],
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

  // try {
  //   log("---> Creating offer");
  //   const offer = await myPeerConnection.createOffer();

  //   // If the connection hasn't yet achieved the "stable" state,
  //   // return to the caller. Another negotiationneeded event
  //   // will be fired when the state stabilizes.

  //   if (myPeerConnection.signalingState != "stable") {
  //     log("     -- The connection isn't stable yet; postponing...")
  //     return;
  //   }

  //   // Establish the offer as the local peer's current
  //   // description.

  //   await myPeerConnection.setLocalDescription(offer);
  //   console.log("---> Setting local description: ", myPeerConnection.localDescription);

  //   // Send the offer to the remote peer.
  //   boxLog(`Sending WebRTC offer to device`);

  //   sendToServer({
  //     type: 0,
  //     data: JSON.stringify(myPeerConnection.localDescription)
  //   });
  // } catch (err) {
  //   log("*** The following error occurred while handling the negotiationneeded event:");
  //   reportError(err);
  // };
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
  // await video.play();
  // video.srcObject = stream;
}

// Handles |icecandidate| events by forwarding the specified
// ICE candidate (created by our local ICE agent) to the other
// peer through the signaling server.
function handleICECandidateEvent(event) {
  if (event.candidate) {
    log("*** Outgoing ICE candidate: " + event.candidate.candidate);

    sendToServer({
      type: 2,
      data: JSON.stringify(event.candidate)
    });
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


async function handleLoginResponse(msg) {
  sendToServer({ type: 3 });
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
  let datachannel = myPeerConnection.createDataChannel("coap");

  datachannel.addEventListener("open", (event) => {
    console.log("Datachannel Opened");
    boxLog("Datachannel Opened");
    let req = {
      type: 0,
      requestId: "foobar",
      method: "GET",
      path: "/p2p/endpoints"
    };
    datachannel.send(JSON.stringify(req));
  });

  datachannel.addEventListener("message", (event) => {
    console.log("Got datachannel message: ", event.data);
    boxLog(`Data channel message recieved: ${event.data}`);

  });

  myPeerConnection.addTransceiver("video", {direction: "recvonly", streams: []});
  const offer = await myPeerConnection.createOffer();

  if (myPeerConnection.signalingState != "stable") {
    log("     -- The connection isn't stable yet; WTF...")
  }

  await myPeerConnection.setLocalDescription(offer);
  sendToServer({
    type: 0,
    data: JSON.stringify(myPeerConnection.localDescription)
  });
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

// Handle a click on an item in the user list by inviting the clicked
// user to video chat. Note that we don't actually send a message to
// the callee here -- calling RTCPeerConnection.addTrack() issues
// a |notificationneeded| event, so we'll let our handler for that
// make the offer.

// async function invite() {
//   log("Starting to prepare an invitation");
//   if (myPeerConnection) {
//     alert("You can't start a call because you already have one open!");
//   } else {

//     createPeerConnection();
//     // log("Asking device " + targetUsername + " for video-offer");
//     // sendToServer({
//     //   name: myUsername,
//     //   target: targetUsername,
//     //   type: "get-feed"
//     // });
//   }
// }

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

  // We need to set the remote description to the received SDP offer
  // so that our local WebRTC layer knows how to talk to the caller.

  var desc = new RTCSessionDescription(msg.data);

  // If the connection isn't stable yet, wait for it...

  // if (myPeerConnection.signalingState != "stable") {
  //   log("  - But the signaling state isn't stable, so triggering rollback");

  //   // Set the local and remove descriptions for rollback; don't proceed
  //   // until both return.
  //   await Promise.all([
  //     myPeerConnection.setLocalDescription({ type: "rollback" }),
  //     myPeerConnection.setRemoteDescription(desc)
  //   ]);
  //   return;
  // }

  log("  - Setting remote description");
  await myPeerConnection.setRemoteDescription(desc);
  await myPeerConnection.setLocalDescription(await myPeerConnection.createAnswer());

  sendToServer({
    type: 1,
    data: JSON.stringify(myPeerConnection.localDescription)
  });
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
