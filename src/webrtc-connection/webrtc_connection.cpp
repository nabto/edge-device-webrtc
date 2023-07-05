#include "webrtc_connection.hpp"
#include "webrtc_util.hpp"

#include <signaling-stream/signaling_stream.hpp>

#include <nlohmann/json.hpp>

#include <iostream>

namespace nabto {

WebrtcConnectionPtr WebrtcConnection::create(SignalingStreamPtr sigStream, NabtoDeviceImplPtr device, std::vector<struct TurnServer>& turnServers)
{
    return std::make_shared<WebrtcConnection>(sigStream, device, turnServers);
}

WebrtcConnection::WebrtcConnection(SignalingStreamPtr sigStream, NabtoDeviceImplPtr device, std::vector<struct TurnServer>& turnServers)
    : sigStream_(sigStream), device_(device), turnServers_(turnServers)
{

}

WebrtcConnection::~WebrtcConnection()
{
    if (nabtoConnection_) {
        nabto_device_virtual_connection_free(nabtoConnection_);
    }
}

void WebrtcConnection::stop()
{
    sigStream_ = nullptr;
}

void WebrtcConnection::handleOffer(std::string& data)
{
    std::cout << "Got Offer: " << data << std::endl;
    if (!pc_) {
        createPeerConnection();
    }
    // TODO: handle json exceptions
    nlohmann::json sdp = nlohmann::json::parse(data);
    rtc::Description remDesc(sdp["sdp"].get<std::string>(), sdp["type"].get<std::string>());
    try {
        pc_->setRemoteDescription(remDesc);
    }
    catch (std::invalid_argument& ex) {
        std::cout << "GOT INVALID ARGUMENT: " << ex.what() << std::endl;
    }

}

void WebrtcConnection::handleAnswer(std::string& data)
{
    std::cout << "Got Answer: " << data << std::endl;
    // TODO: handle json exceptions
    nlohmann::json sdp = nlohmann::json::parse(data);
    rtc::Description remDesc(sdp["sdp"].get<std::string>(), sdp["type"].get<std::string>());
    pc_->setRemoteDescription(remDesc);
}

void WebrtcConnection::handleIce(std::string& data)
{
    std::cout << "Got ICE: " << data << std::endl;
    // TODO: handle json exceptions
    nlohmann::json candidate = nlohmann::json::parse(data);
    rtc::Candidate cand(candidate["candidate"].get<std::string>(), candidate["sdpMid"].get<std::string>());
    pc_->addRemoteCandidate(cand);
}

void WebrtcConnection::createPeerConnection()
{
    auto self = shared_from_this();
    rtc::Configuration conf;

    if (!webrtc_util::parseTurnServers(conf, turnServers_)) {
        std::cout << "Failed to parce TURN server configurations" << std::endl;
        state_ = FAILED;
        // TODO: handle error states
    }
    // conf.iceTransportPolicy = rtc::TransportPolicy::Relay;
    conf.disableAutoNegotiation = true;
    pc_ = std::make_shared<rtc::PeerConnection>(conf);

    pc_->onStateChange([self](rtc::PeerConnection::State state) {
        std::cout << "State: " << state << std::endl;
        if (state == rtc::PeerConnection::State::Connected) {
            self->state_ = CONNECTED;
        } else if (state == rtc::PeerConnection::State::Closed) {
            self->state_ = CLOSED;
            // TODO: handle closure
            self->sigStream_ = nullptr;
        }
    });

    pc_->onSignalingStateChange(
        [self](rtc::PeerConnection::SignalingState state) {
            std::cout << "Signalling State: " << state << std::endl;
            self->handleSignalingStateChange(state);
        });

    pc_->onLocalCandidate([self](rtc::Candidate cand) {
        std::cout << "Got local candidate: " << cand << std::endl;
        nlohmann::json candidate;
        candidate["sdpMid"] = cand.mid();
        candidate["candidate"] = cand.candidate();
        auto data = candidate.dump();

        // TODO: add metadata
        nlohmann::json metadata;

        self->sigStream_->signalingSendIce(data, metadata);
    });

    pc_->onTrack([self](std::shared_ptr<rtc::Track> track) {
        std::cout << "Got Track event" << std::endl;
        self->handleTrackEvent(track);
    });

    pc_->onDataChannel([self](std::shared_ptr<rtc::DataChannel> incoming) {
        std::cout << "Got new datachannel: " << incoming->label() << std::endl;
        self->handleDatachannelEvent(incoming);
    });

    pc_->onGatheringStateChange(
        [self](rtc::PeerConnection::GatheringState state) {
            std::cout << "Gathering State: " << state << std::endl;
        });

}

void WebrtcConnection::handleSignalingStateChange(rtc::PeerConnection::SignalingState state)
{
    if (state ==
        rtc::PeerConnection::SignalingState::HaveLocalOffer) {
    } else if (state ==
               rtc::PeerConnection::SignalingState::HaveRemoteOffer) {
        try {
            pc_->setLocalDescription();
        }
        catch (std::logic_error ex) {
            // TODO: handle this
            std::cout << "EXCEPTION!!!! " << ex.what() << std::endl;
        }
    } else {
        std::cout << "Got unhandled signaling state: " << state << std::endl;
        return;
    }
    auto description = pc_->localDescription();
    nlohmann::json message = {
        {"type", description->typeString()},
        {"sdp", std::string(description.value())} };
    auto data = message.dump();
    // TODO: add metadata
    nlohmann::json metadata;
    if (description->type() == rtc::Description::Type::Offer) {
        sigStream_->signalingSendOffer(data, metadata);
    } else {
        sigStream_->signalingSendAnswer(data, metadata);
    }

}

void WebrtcConnection::handleTrackEvent(std::shared_ptr<rtc::Track> track)
{
    // TODO: refactor this code
    std::cout << "Got Track event" << std::endl;
    try {
        auto media = track->description();
        rtc::Description::Media::RtpMap* rtp = NULL;
        for (auto pt : media.payloadTypes()) {
            rtc::Description::Media::RtpMap* r = NULL;
            try {
                r = media.rtpMap(pt);
            }
            catch (std::exception& ex) {
                // std::cout << "Bad rtpMap for pt: " << pt << std::endl;
                continue;
            }
            // TODO: make codec configureable and generalize this matching
            std::string profLvlId = "42e01f";
            // std::string lvlAsymAllowed = "1";
            // std::string pktMode = "1";
            // std::string profLvlId = "4d001f";
            std::string lvlAsymAllowed = "1";
            std::string pktMode = "1";
            if (r != NULL && r->fmtps.size() > 0 &&
                r->fmtps[0].find("profile-level-id=" + profLvlId) != std::string::npos &&
                r->fmtps[0].find("level-asymmetry-allowed=" + lvlAsymAllowed) != std::string::npos &&
                r->fmtps[0].find("packetization-mode=" + pktMode) != std::string::npos
                ) {
                std::cout << "FOUND RTP codec match!!! " << pt << std::endl;
                rtp = r;
            }
            else {
                // std::cout << "no match, removing" << std::endl;
                media.removeRtpMap(pt);
            }
        }
        // TODO: handle no match found error


        media.addSSRC(42, "video-send");
        auto track_ = pc_->addTrack(media);
    }
    catch (std::exception ex) {
        std::cout << "GOT EXCEPTION!!! " << ex.what() << std::endl;
    }

}

void WebrtcConnection::handleDatachannelEvent(std::shared_ptr<rtc::DataChannel> incoming)
{
    if (incoming->label() == "coap") {
        if (nabtoConnection_ == NULL) {
            nabtoConnection_ = nabto_device_virtual_connection_new(device_->getDevice());

        }
        coapChannel_ = WebrtcCoapChannel::create(incoming, device_, nabtoConnection_);
    }

}


} // namespace
