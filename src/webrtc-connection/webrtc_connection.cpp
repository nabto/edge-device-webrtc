#include "webrtc_connection.hpp"
#include "webrtc_util.hpp"

#include <signaling-stream/signaling_stream.hpp>

#include <nlohmann/json.hpp>

#include <iostream>

namespace nabto {

WebrtcConnectionPtr WebrtcConnection::create(SignalingStreamPtr sigStream, NabtoDeviceImplPtr device, std::vector<struct TurnServer>& turnServers, std::vector<nabto::MediaStreamPtr>& medias)
{
    return std::make_shared<WebrtcConnection>(sigStream, device, turnServers, medias);
}

WebrtcConnection::WebrtcConnection(SignalingStreamPtr sigStream, NabtoDeviceImplPtr device, std::vector<struct TurnServer>& turnServers, std::vector<nabto::MediaStreamPtr>& medias)
    : sigStream_(sigStream), device_(device), turnServers_(turnServers), medias_(medias)
{

}

WebrtcConnection::~WebrtcConnection()
{
    std::cout << "WebrtcConnection Destructor" << std::endl;
    if (nabtoConnection_) {
        nabto_device_virtual_connection_free(nabtoConnection_);
    }
}

void WebrtcConnection::stop()
{
    // sigStream_ = nullptr;
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
        if (self->eventHandler_) {
            self->eventHandler_(self->state_);
        }
        // TODO: handle error states
    }
    // conf.iceTransportPolicy = rtc::TransportPolicy::Relay;
    conf.disableAutoNegotiation = true;
    pc_ = std::make_shared<rtc::PeerConnection>(conf);

    pc_->onStateChange([self](rtc::PeerConnection::State state) {
        std::cout << "State: " << state << std::endl;
        if (state == rtc::PeerConnection::State::Connected) {
            self->state_ = CONNECTED;
            if (self->eventHandler_) {
                self->eventHandler_(self->state_);
            }
        } else if (state == rtc::PeerConnection::State::Connecting) {
            self->state_ = CONNECTING;
            if (self->eventHandler_) {
                self->eventHandler_(self->state_);
            }

        } else if (state == rtc::PeerConnection::State::Closed) {
            self->state_ = CLOSED;
            if (self->eventHandler_) {
                self->eventHandler_(self->state_);
                self->eventHandler_ = nullptr;
            }
            for (auto m : self->medias_) {
                m->removeConnection(self->pc_);
            }
            // TODO: handle closure
            // self->sigStream_ = nullptr;
            self->coapChannel_ = nullptr;
            self->pc_->close();
            self->pc_ = nullptr;
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
    // TODO: construct metadata if we are making the offer
    if (description->type() == rtc::Description::Type::Offer) {
        sigStream_->signalingSendOffer(data, metadata_);
    } else {
        sigStream_->signalingSendAnswer(data, metadata_);
    }

}

void WebrtcConnection::handleTrackEvent(std::shared_ptr<rtc::Track> track)
{
    std::cout << "Track event metadata: " << metadata_.dump() << std::endl;

    auto mid = track->mid();

    // TODO: handle json exceptions
    auto metaTracks = metadata_["tracks"].get<std::vector<nlohmann::json>>();
    for (auto mt : metaTracks) {
        if (mt["mid"].get<std::string>() == mid) {
            std::cout << "Found metaTrack: " << mt.dump() << std::endl;
            auto trackId = mt["trackId"].get<std::string>();
            for (auto m : medias_) {
                if (m->getAudioTrackId() == trackId) {
                    m->addAudioTrack(track, pc_);
                    return;
                } else if (m->getVideoTrackId() == trackId) {
                    m->addVideoTrack(track, pc_);
                    return;
                }
            }
        }
    }
    // TODO: handle not found error
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
