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
var singleOffer = false;
var myPeerConnection = null;    // RTCPeerConnection
var transceiver = null;         // RTCRtpTransceiver
var webcamStream = null;        // MediaStream from webcam
let metadata = undefined;
let connected = false;
let audioSender = null;
let localStream;

var iceServers = [{ urls: "stun:stun.nabto.net" }]; // Servers to use for Turn/stun

var pictureBuffer = new ArrayBuffer();

function appendBuffer(buffer)
{
  var tmp = new Uint8Array( buffer.byteLength + pictureBuffer.byteLength );
  tmp.set( new Uint8Array( pictureBuffer ), 0 );
  tmp.set( new Uint8Array( buffer ), pictureBuffer.byteLength );
  pictureBuffer = tmp.buffer;
}

function bufferToBase64( buffer ) {
  var binary = '';
  var bytes = new Uint8Array( buffer );
  var len = bytes.byteLength;
  for (var i = 0; i < len; i++) {
      binary += String.fromCharCode( bytes[ i ] );
  }
  return window.btoa( binary );
}

async function getImage()
{

  const constraints = window.constraints = {
    audio: true,
    video: false
  };
  const stream = await navigator.mediaDevices.getUserMedia(constraints);

  var audio = document.getElementById("received_audio");
  audio.srcObject = stream;

  return;
  nabtoConnection.coapInvoke("GET", "/webrtc/info", undefined, undefined, (response) => {
    let resp = JSON.parse(response);
    boxLog("Got /webrtc/info coap response: " + JSON.stringify(resp));
    if (resp.statusCode != 205) {
      boxLog("Failed to get WebRTC info from device: " + resp.statusCode);
      return;
    }
    try {
      let payloadObj = nabtoConnection.decodeCborPayload(resp.payload);
      let streamPort = payloadObj.FileStreamPort;

      // TODO: ephemeral stream port
      console.log("Opening stream to port: ", streamPort);
      let stream = myPeerConnection.createDataChannel("stream-"+streamPort);
      nabtoConnection.readStream(stream, (data) => {
        console.log("read stream data");
        appendBuffer(data);
      });
      stream.addEventListener("close", (event) => {
        console.log("Stream channel closed event. buffer length: ", pictureBuffer.byteLength);

        img = document.getElementById("nabtoimage");
        img.src = 'data:image/png;base64,' + bufferToBase64(pictureBuffer);

      });
    } catch (err) {
      console.log("ERROR: ", err);
    }
  });

}

function addAudio() {
  addAudioTrack();
}

function tokenUpdated() {
  if (document.getElementById("oauthtoken").value.length > 0 && connected) {
    document.getElementById("oauth").disabled = false;
  } else {
    document.getElementById("oauth").disabled = true;
  }

}

function oAuth() {
  let token = document.getElementById("oauthtoken").value;
//  "eyJhbGciOiJSUzI1NiIsInR5cCI6ImF0K2p3dCIsImtpZCI6ImtleXN0b3JlLUNIQU5HRS1NRSJ9.eyJqdGkiOiJoOFp1b1B5b3RqMWZnNnpmT2tzYlciLCJpYXQiOjE2OTQxNjYyMzQsImV4cCI6MTY5NDE2OTgzNCwiaXNzIjoiaHR0cDovL2xvY2FsaG9zdDozMDAwIiwiYXVkIjoibmFidG86Ly9kZXZpY2U_cHJvZHVjdElkPXByLTAmZGV2aWNlSWQ9ZGUtMCJ9.HErS2xDBPvADPKi_W6Apr9x0Iw88qZMMYJjshkC3K7Uq-zxhD7bX09hMKK6hoFA7msv4A6UDoujn55wQp-i4n1TnqJ2zSxAu1BtSMtK55MaL5dmKi8cU9QP7PZGHDZKngVo4ntybwkw2a0S2vQofy2-I91ZbMjRBQHH4R_xCrSOITy-4BuTn5SoMZex88RLW--67yBxwaNfiIc0tzfRDmE72CQVC_1nIX9qdvAx0gcS2rhKG3DmGho2foX5wA8hIViGQIy5PNwwqLp_qBBRS7CeOs4a1R-ElsljmUWJwqlbvR4Ps0mPyD0-sk542rRbY3mtPAo8kapGMKgifG1CP3Q";
  nabtoConnection.coapInvoke("POST", "/webrtc/oauth", 0, token, (response) => {
    let resp = JSON.parse(response);
    if (resp.statusCode != 201) {
      boxLog("Failed perform Oauth login: " + resp.statusCode);
    } else {
      boxLog("Oauth login successful");
    }

  });
}

function passAuth() {
  let uname = document.getElementById("iamuser").value;
  let pwd = document.getElementById("iampwd").value;
  nabtoConnection.passwordAuthenticate(uname, pwd, (success) => {
    if (success) {
      boxLog("Successfully authenticated using password");
    } else {
      boxLog("Failed to authenticate using password");
    }
  });

}

function setPassword() {
  let uname = document.getElementById("iamuser").value;
  let pwd = document.getElementById("iampwd").value;
  if (pwd == "") {
    pwd = null;
  }
  let payload = nabtoConnection.encodeCborPayload(pwd);
  console.log(payload);
  nabtoConnection.coapInvoke("PUT", `/iam/users/${uname}/password`, 60, payload, async (response) => {
    let resp = JSON.parse(response);
    if (resp.statusCode != 204) {
      boxLog("Failed set password: " + resp.statusCode);
    } else {
      boxLog("New password set");
    }
  });
}

function validateFingerprint() {
  let fp = document.getElementById("fp").value;
  let payload = {
    challenge: nabtoConnection.uuidv4()
  }
  nabtoConnection.coapInvoke("POST", "/webrtc/challenge", 50, JSON.stringify(payload), async (response) => {
    let resp = JSON.parse(response);
    if (resp.statusCode != 205) {
      boxLog("Failed validate fingerprint: " + resp.statusCode);
    } else {
      let respPl = JSON.parse(String.fromCharCode.apply(null, resp.payload));
      console.log("respPl: ", respPl);
      let valid = await nabtoConnection.validateJwt(respPl.response, fp);
      console.log("Fingerprint validity: ", valid);
      if (valid) {
        boxLog("Device fingerprint validated successfully");
      } else {
        boxLog("Device fingerprint FAILED!");
      }
    }
  });
}

function requestVideo() {
  let payload = {
    tracks: ["frontdoor-video", "frontdoor-audio"]
  }
  nabtoConnection.coapInvoke("POST", "/webrtc/tracks", 50, JSON.stringify(payload), (response) => {
    boxLog("Got coap response: " + response);
  });

}

// Open a websocket connection to the signaling server and make it connect to the device.
function connect() {
  reset();

  // Remove this line to test locally (defaults to "ws://localhost:6503")
  // nabtoSignaling.setSignalingHost("ws://63.32.24.220:6503");
  nabtoSignaling.setSignalingHost("wss://signaling.smartcloud.nabto.com");

  console.log(`Connecting to server: ${nabtoSignaling.signalingHost}`);
  boxLog(`Connecting to server: ${nabtoSignaling.signalingHost}`);

  // Get the productId, deviceId, and SCT from the HTML-page
  productId = document.getElementById("product").value;
  deviceId = document.getElementById("device").value;
  sct = document.getElementById("sct").value;
  let singleOfferElm = document.getElementById("singleoffer");
  singleOffer = singleOfferElm.checked;

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

    // Create the RTCPeerConnection for WebRTC
    createPeerConnection();

    // We create a data channel in the peer connection to use for coap invocations
    let coapChannel = myPeerConnection.createDataChannel("coap");

    if (singleOffer) {
      // We also want to receive a video feed from the device
      let transceiver = myPeerConnection.addTransceiver("video", {direction: "recvonly", streams: []});

      const constraints = window.constraints = {
        audio: true,
        video: false
      };
      const stream = await navigator.mediaDevices.getUserMedia(constraints);
      localStream = stream;
      localStream.getTracks().forEach(track => myPeerConnection.addTrack(track, localStream));

      // let audioTrans = myPeerConnection.addTransceiver("audio", {direction: "recvonly", streams: []});
      let offer = await myPeerConnection.createOffer();
      await myPeerConnection.setLocalDescription(offer);

      // TODO: when adding the audio track with we do not get a transceiver obj so we just quess the mid is 1
      metadata = {
          tracks: [
            {
              mid: transceiver.mid,
              trackId: "frontdoor-video"
            }
            ,
            {
              mid: "1",
              trackId: "frontdoor-audio"
            }
          ],
          noTrickle: false
        };
    } else {
      let offer = await myPeerConnection.createOffer();
      await myPeerConnection.setLocalDescription(offer);
    }

    // Create an offer with the data channel and video channel

    // Set the local description using the offer

    // let metadata = {
    //   tracks: [
    //     {
    //       mid: transceiver.mid,
    //       trackId: "frontdoor-video"
    //     }
    //   ]
    // }
    // let metadata = {
    //   tracks: [
    //     {
    //       mid: "0",
    //       trackId: "frontdoor-video"
    //     }
    //   ]
    // }

    // Send the local description as our offer to the device through the signaling channel
    nabtoSignaling.sendOffer(myPeerConnection.localDescription, metadata);
    // Once the offer is sent, we wait for the device to send us an answer.
    // In the mean time, we send any ICE candidates the RTCPeerConnection finds to the device.

    // When the WebRTC connection is established and the data channel is opened we want to be notified.
    coapChannel.addEventListener("open", (event) => {
      connectedState(true);
      boxLog("Datachannel Opened");
      // We add the data channel to the nabtoConnection so it can use it for coap requests
      nabtoConnection.setCoapDataChannel(coapChannel);

      // if (!singleOffer) {
      //   nabtoConnection.passwordAuthenticate("admin", "demoAdminPwd", (success) => {
      //     if (success) {
      //       boxLog("Successfully logged in as admin");
      //       nabtoConnection.coapInvoke("GET", "/webrtc/video/frontdoor", undefined, undefined, (response) => {
      //         boxLog("Got coap response: " + response);
      //       });
      //     } else {
      //       boxLog("Failed to do password login, NOT OPENING STREAM");
      //     }
      //   });
      //   // nabtoConnection.coapInvoke("GET", "/webrtc/video/frontdoor-audio", undefined, undefined, (response) => {
      //   //   boxLog("Got coap response: " + response);
      //   // });
      // } else {
      //   // nabtoConnection.passwordAuthenticate("foo", "bar", (success) => {
      //   //   if (success) {
      //   //     boxLog("Successfully authenticated using password");
      //   //   } else {
      //   //     boxLog("Failed to authenticate using password");
      //   //   }
      //   // });
      // }

    });


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
  nabtoConnection.setPeerConnection(myPeerConnection);
}



async function addAudioTrack()
{
  const constraints = window.constraints = {
    audio: true,
    video: false
  };
  const stream = await navigator.mediaDevices.getUserMedia(constraints);
  localStream = stream;
  // event.transceiver.sender.replaceTrack(event.track);
  // event.transceiver.sender.setStreams(stream);
  const audioTracks = localStream.getAudioTracks();
  if (audioTracks.length > 0) {
    console.log(`Using Audio device: ${audioTracks[0].label}`);
  }
  localStream.getTracks().forEach(track => myPeerConnection.addTrack(track, localStream));
  let offer = await myPeerConnection.createOffer();
  await myPeerConnection.setLocalDescription(offer);
  nabtoSignaling.sendOffer(myPeerConnection.localDescription, metadata);


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
    console.log("Not video track, must be audio");
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
}

// Handle |iceconnectionstatechange| events. This will detect
// when the ICE connection is closed, failed, or disconnected.
//
// This is called when the state of the ICE agent changes.

function handleICEConnectionStateChangeEvent(event) {
  console.log("*** ICE connection state changed to " + myPeerConnection.iceConnectionState);

  switch (myPeerConnection.iceConnectionState) {
    case "connected":
      myPeerConnection.getTransceivers().forEach(transceiver => {
        console.log("transceiver: ", transceiver);
        if (transceiver.currentDirection == "inactive") {
          boxLog("Peer Connection established, but transceiver mid: " + transceiver.mid + " kind: " + transceiver.receiver.track.kind + " is inactive");
          boxLog("You may not have the proper IAM permissions to access this track");
        }
      });

      break;
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
  connectedState(false);

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
  metadata = undefined;
  connectedState(false);

}

// Called by the WebRTC layer to let us know when it's time to
// begin, resume, or restart ICE negotiation.

async function handleNegotiationNeededEvent() {
  console.log("*** Negotiation needed");
  // TODO: renegotiation

}

function connectedState(isConn) {
  connected = isConn;
  tokenUpdated();
  document.getElementById("reqvid").disabled = !isConn;
  document.getElementById("addaudio").disabled = !isConn;
  document.getElementById("passauth").disabled = !isConn;
  document.getElementById("setpass").disabled = !isConn;
  document.getElementById("fpvalid").disabled = !isConn;
  // document.getElementById("getimage").disabled = !isConn;
  document.getElementById("login").disabled = isConn;

}
