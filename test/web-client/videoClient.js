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


// WebRTC signalling object used opening and invoking the WebSocket connection to the signaling server
var nabtoSignaling = new window.NabtoWebrtcSignaling();
// nabto connection object used to make native Nabto CoAP requests and Streams to the device using WebRTC data channels
var nabtoConnection = new window.NabtoWebrtcConnection();

var productId = null;
var deviceId = null;
var sct = null;
var myPeerConnection = null;    // RTCPeerConnection
var transceiver = null;         // RTCRtpTransceiver
var webcamStream = null;        // MediaStream from webcam

var iceServers = [{ urls: "stun:stun.nabto.net" }]; // Servers to use for Turn/stun


// Open a websocket connection to the signaling server and make it connect to the device.
function connect() {
  reset();

  // Remove this line to test locally (defaults to "ws://localhost:6503")
  // nabtoSignaling.setSignalingHost("ws://34.245.62.208:6503");

  console.log(`Connecting to server: ${nabtoSignaling.signalingHost}`);
  boxLog(`Connecting to server: ${nabtoSignaling.signalingHost}`);

  // Get the productId, deviceId, and SCT from the HTML-page
  productId = document.getElementById("product").value;
  deviceId = document.getElementById("device").value;
  sct = document.getElementById("sct").value;

  // Set the device configuration in the signaling
  nabtoSignaling.setDeviceConfigSct(productId, deviceId, sct);

  // Set callback when a signaling connection to the device is established
  nabtoSignaling.onconnected = (msg) => {
    // An RTCPeerConnection takes TURN server configuration in its constructor, so when the signaling connection is established we start by requesting TURN server credentials.
    nabtoSignaling.requestTurnCredentials();
  };

  // Callback when TURN credentials are received from the device
  nabtoSignaling.onturncredentials = async (msg) => {
    // handleTurnResponse() goes through the response, adds servers to our config, and writes log
    handleTurnResponse(msg);

    nabtoSignaling.sendToServer({
      type: 5
    });
    return;

    // Create the RTCPeerConnection for WebRTC
    createPeerConnection();

    // We create a data channel in the peer connection to use for coap invocations
    // let coapChannel = myPeerConnection.createDataChannel("coap");

    // // We also want to receive a video feed from the device
    // let transceiver = myPeerConnection.addTransceiver("video", {direction: "recvonly", streams: []});
    // // myPeerConnection.addTransceiver("video", {direction: "recvonly", streams: []});

    // Create an offer with the data channel and video channel
    const offer = await myPeerConnection.createOffer();

    // Set the local description using the offer
    // await myPeerConnection.setLocalDescription(offer);

    // let metadata = {
    //   tracks: [
    //     {
    //       mid: transceiver.mid,
    //       trackId: "frontdoor-video"
    //     }
    //   ]
    // }
    let metadata = {
      tracks: [
        {
          mid: "0",
          trackId: "frontdoor-video"
        }
      ]
    }

    // Send the local description as our offer to the device through the signaling channel
    nabtoSignaling.sendOffer(myPeerConnection.localDescription, metadata);
    // Once the offer is sent, we wait for the device to send us an answer.
    // In the mean time, we send any ICE candidates the RTCPeerConnection finds to the device.

    // When the WebRTC connection is established and the data channel is opened we want to be notified.
    // coapChannel.addEventListener("open", (event) => {
    //   boxLog("Datachannel Opened");
    //   // We add the data channel to the nabtoConnection so it can use it for coap requests
    //   nabtoConnection.setCoapDataChannel(coapChannel);

    //   nabtoConnection.coapInvoke("GET", "/p2p/endpoints", undefined, undefined, (response) => {
    //     boxLog("Got coap response: " + response);
    //   });
    //   // nabtoConnection.passwordAuthenticate("foo", "bar", (success) => {
    //   //   if (success) {
    //   //     boxLog("Successfully authenticated using password");
    //   //   } else {
    //   //     boxLog("Failed to authenticate using password");
    //   //   }
    //   // });

    // });


  };
  // Callback when the device answers our offer.
  nabtoSignaling.onanswer = async (msg) => {
    boxLog(`Recieved WebRTC answer from device!`);
    let answer = JSON.parse(msg.data);
    console.log("Received and parsed answer: ", answer);
    // We constructs an description from the received answer, and sets it as the remote description on the RTCPeerConnection
    var desc = new RTCSessionDescription(answer);
    await myPeerConnection.setRemoteDescription(desc).catch((reason)=> {
      console.log("reason: ", reason);
      console.log("ERROR: ", reason.code);
      console.log("ERROR: ", reason.name);
      closeVideoCall();

    });
  };

  // Callback when device sends ICE candidates in trickle ICE mode.
  nabtoSignaling.onicecandidate = async (msg) => {
    console.log("*** Adding received ICE candidate: ", msg.data);
    try {
      let cand = JSON.parse(msg.data);
      // When we receive an ICE candidate from the device, we parse it and adds it to the RTCPeerConnection
      var candidate = new RTCIceCandidate(cand);
      await myPeerConnection.addIceCandidate(candidate)
    } catch (err) {
      console.log("failed to add candidate to peer connection", err);
    }
  };

  // Callback if the device sends us an offer. Since we initiate the connection, this will only be invoked if the device wants to renegotiate the connection.
  nabtoSignaling.onoffer = (msg) => {
    handleVideoOfferMsg(msg);
  };

  nabtoSignaling.onerror = (msg) => {
    console.log("error: ", msg);
    reset();
  }


  nabtoSignaling.signalingConnect();
}


function createPeerConnection() {
  console.log("Setting up a WebRTC connection...");

  // Create an RTCPeerConnection which knows to use our chosen
  // STUN server.

  myPeerConnection = new RTCPeerConnection({
    iceServers: iceServers,
    // iceTransportPolicy: "relay",
  });

  // When the RTCPeerConnection finds an ICE candidate, we send it to the device through the signaling channel.
  myPeerConnection.onicecandidate = (event) => {
    if (event.candidate) {
      console.log("*** Outgoing ICE candidate: " + event.candidate.candidate);
      nabtoSignaling.sendIceCandidate(event.candidate);
    }

  };

  myPeerConnection.ontrack = handleTrackEvent;

  myPeerConnection.oniceconnectionstatechange = handleICEConnectionStateChangeEvent;
  myPeerConnection.onicegatheringstatechange = handleICEGatheringStateChangeEvent;
  myPeerConnection.onsignalingstatechange = handleSignalingStateChangeEvent;
  myPeerConnection.onnegotiationneeded = handleNegotiationNeededEvent;
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
  console.log("*** Track event. streams length: " + event.streams.length);
  console.log("event: ", event);
  console.log("event.track: ", event.track);
  console.log("event.streams: ", event.streams);
  var stream = event.streams[0];
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

// Handle |iceconnectionstatechange| events. This will detect
// when the ICE connection is closed, failed, or disconnected.
//
// This is called when the state of the ICE agent changes.

function handleICEConnectionStateChangeEvent(event) {
  console.log("*** ICE connection state changed to " + myPeerConnection.iceConnectionState);

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
  console.log("*** WebRTC signaling state changed to: " + myPeerConnection.signalingState);
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
  console.log("*** ICE gathering state changed to: " + myPeerConnection.iceGatheringState);
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

}

// Close the RTCPeerConnection and reset variables so that the user can
// make or receive another call if they wish. This is called both
// when the user hangs up, the other user hangs up, or if a connection
// failure is detected.

function closeVideoCall() {
  boxLog("Closing the call");

  // Close the RTCPeerConnection

  if (myPeerConnection) {
    console.log("--> Closing the peer connection");

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
    nabtoSignaling.close();
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

  // nabtoSignaling.setSignalingHost("ws://34.245.62.208:6503");

  boxLog(`Recieved WebRTC offer from device! Sending answer`);
  console.log("Received video chat offer With SDP: ", msg.data);
  if (!myPeerConnection) {
    createPeerConnection();
  }

  let offer = JSON.parse(msg.data);

  var desc = new RTCSessionDescription(offer);

  console.log("  - Setting remote description");
  await myPeerConnection.setRemoteDescription(desc);
  await myPeerConnection.setLocalDescription(await myPeerConnection.createAnswer());

  nabtoSignaling.sendAnswer(myPeerConnection.localDescription);
}

function boxLog(msg) {
  var listElem = document.querySelector(".logbox");

  var item = document.createElement("li");
  item.appendChild(document.createTextNode(msg));
  listElem.appendChild(item);
}

function reset() {
  nabtoSignaling = new window.NabtoWebrtcSignaling();
  nabtoConnection = new window.NabtoWebrtcConnection();
  myPeerConnection = null;    // RTCPeerConnection
  transceiver = null;         // RTCRtpTransceiver
  webcamStream = null;        // MediaStream from webcam

}

// Called by the WebRTC layer to let us know when it's time to
// begin, resume, or restart ICE negotiation.

async function handleNegotiationNeededEvent() {
  console.log("*** Negotiation needed");
  // TODO: renegotiation
}

