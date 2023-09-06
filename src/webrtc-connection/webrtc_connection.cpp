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
    if (pc_) {
        pc_->close();
    }
    // sigStream_ = nullptr;
}

void WebrtcConnection::handleOfferRequest()
{
        std::cout << "Got Offer request: " << std::endl;
    if (!pc_) {
        createPeerConnection();
    }

    auto chan = pc_->createDataChannel("coap");
    handleDatachannelEvent(chan);

    for (auto m : medias_) {
        m->createTrack(pc_);
    }
    pc_->setLocalDescription();


}

void WebrtcConnection::handleOffer(std::string& data)
{
    std::cout << "Got Offer: " << data << std::endl;
    if (!pc_) {
        createPeerConnection();
    }
    try {
        nlohmann::json sdp = nlohmann::json::parse(data);
        rtc::Description remDesc(sdp["sdp"].get<std::string>(), sdp["type"].get<std::string>());

        if (remDesc.hasAudioOrVideo()) {
            NabtoDeviceConnectionRef ref = (nabtoConnection_ == NULL ? sigStream_->getSignalingConnectionRef() : nabto_device_connection_get_connection_ref(nabtoConnection_));
            if (!nm_iam_check_access(device_->getIam(), ref, "Webrtc:VideoStream", NULL)) {
                std::cout << "  Offer contained video Track rejected by IAM" << std::endl;
                int c = remDesc.mediaCount();
                for (int i = 0; i < c; i++) {
                    if (rtc::holds_alternative<rtc::Description::Media*>(remDesc.media(i))) {
                        auto m = rtc::get<rtc::Description::Media*>(remDesc.media(i));
                        if (m->type() == "video" || m->type() == "audio") {
                            m->setDirection(rtc::Description::Direction::Inactive);
                            std::cout << "    setting media of type " << m->type() << " to InActive" << std::endl;
                        } else {
                            std::cout << "    Unknown media type: " << m->type() << std::endl;
                        }
                    }

                }
            } else if (tracks_.size() != 0){
                // If we already have a remote description in pc_
                //   And both the incoming and current description has the video feed
                //   And the existing feed has direction "inactive"
                //   And IAM has allowed the video feed
                // Then we must activate the existing feeds
                for (auto t : tracks_) {
                    if (t->direction() == rtc::Description::Direction::Inactive) {
                        auto desc = t->description();
                        if (desc.type() == "video") {
                            desc.setDirection(rtc::Description::Direction::SendOnly);
                        } else {
                            desc.setDirection(rtc::Description::Direction::SendRecv);
                        }
                        t->setDescription(desc);

                        acceptTrack(t);
                    }
                }
            }

        }

        // std::cout << "Setting remDesc: " << remDesc << std::endl;
        pc_->setRemoteDescription(remDesc);

        for (auto t : tracks_) {
            if (t->direction() == rtc::Description::Direction::Inactive) {
                std::cout << "Track " << t->mid() << " inactive after set remote description" << std::endl;
            }
        }
    }
    catch (std::invalid_argument& ex) {
        std::cout << "GOT INVALID ARGUMENT: " << ex.what() << std::endl;
    }
    catch (nlohmann::json::exception& ex) {
        std::cout << "handleOffer json exception: " << ex.what() << std::endl;
    }

}

void WebrtcConnection::handleAnswer(std::string& data)
{
    std::cout << "Got Answer: " << data << std::endl;
    try {
        nlohmann::json sdp = nlohmann::json::parse(data);
        rtc::Description remDesc(sdp["sdp"].get<std::string>(), sdp["type"].get<std::string>());
        pc_->setRemoteDescription(remDesc);
    }
    catch (nlohmann::json::exception& ex) {
        std::cout << "handleAnswer json exception: " << ex.what() << std::endl;
    }
}

void WebrtcConnection::handleIce(std::string& data)
{
    std::cout << "Got ICE: " << data << std::endl;
    try {
        nlohmann::json candidate = nlohmann::json::parse(data);
        rtc::Candidate cand(candidate["candidate"].get<std::string>(), candidate["sdpMid"].get<std::string>());
        pc_->addRemoteCandidate(cand);
    }
    catch (nlohmann::json::exception& ex) {
        std::cout << "handleIce json exception: " << ex.what() << std::endl;
    }
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
    conf.forceMediaTransport = true;
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
        if (self->canTrickle_) {
            nlohmann::json candidate;
            candidate["sdpMid"] = cand.mid();
            candidate["candidate"] = cand.candidate();
            auto data = candidate.dump();

            // TODO: add metadata
            nlohmann::json metadata;

            self->sigStream_->signalingSendIce(data, metadata);
        }
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
            if (state == rtc::PeerConnection::GatheringState::Complete && !self->canTrickle_) {
                auto description = self->pc_->localDescription();
                nlohmann::json message = {
                    {"type", description->typeString()},
                    {"sdp", std::string(description.value())} };
                auto data = message.dump();
                // TODO: construct metadata if we are making the offer
                if (description->type() == rtc::Description::Type::Offer) {
                    std::cout << "Sending offer: " << std::string(description.value()) << std::endl;
                    self->sigStream_->signalingSendOffer(data, self->metadata_);
                }
                else {
                    std::cout << "Sending answer: " << std::string(description.value()) << std::endl;
                    self->sigStream_->signalingSendAnswer(data, self->metadata_);
                }
            }
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
    if (canTrickle_ || pc_->gatheringState() == rtc::PeerConnection::GatheringState::Complete) {
        auto description = pc_->localDescription();
        nlohmann::json message = {
            {"type", description->typeString()},
            {"sdp", std::string(description.value())} };
        auto data = message.dump();
        // TODO: construct metadata if we are making the offer
        if (description->type() == rtc::Description::Type::Offer) {
            std::cout << "Sending offer: " << std::string(description.value()) << std::endl;
            sigStream_->signalingSendOffer(data, metadata_);
        }
        else {
            std::cout << "Sending answer: " << std::string(description.value()) << std::endl;
            sigStream_->signalingSendAnswer(data, metadata_);
        }
    }

}

void WebrtcConnection::handleTrackEvent(std::shared_ptr<rtc::Track> track)
{
    std::cout << "Track event metadata: " << metadata_.dump() << std::endl;
    tracks_.push_back(track);
    if (track->direction() == rtc::Description::Direction::Inactive) {
        track->onOpen([track]() {
            std::cout << "  TRACK OPENED " << track->mid() << " " << track->direction() << std::endl;
            });
        track->onClosed([track]() {
            std::cout << "  TRACK CLOSED " << track->mid() << " " << track->direction() << std::endl;
            });
        track->onError([track](std::string err) {
            std::cout << "  TRACK ERROR " << err << " " << track->mid() << " " << track->direction() << std::endl;
            });

        std::cout << "  Track was inactive, ignoring" << std::endl;
        return;
    }
    acceptTrack(track);
}

void WebrtcConnection::acceptTrack(std::shared_ptr<rtc::Track> track)
{
    std::cout << "acceptTrack metadata: " << metadata_.dump() << std::endl;
    try {
        auto mid = track->mid();

        auto metaTracks = metadata_["tracks"].get<std::vector<nlohmann::json>>();
        for (auto mt : metaTracks) {
            if (mt["mid"].get<std::string>() == mid) {
                std::cout << "Found metaTrack: " << mt.dump() << std::endl;
                auto trackId = mt["trackId"].get<std::string>();
                for (auto m : medias_) {
                    m->addTrack(track, pc_, trackId);
                }
            }
        }
    }
    catch (nlohmann::json::exception& ex) {
        std::cout << "acceptTrack json exception: " << ex.what() << std::endl;
    }
    // TODO: handle not found error

}

void WebrtcConnection::handleDatachannelEvent(std::shared_ptr<rtc::DataChannel> incoming)
{
    if (incoming->label() == "coap") {
        if (nabtoConnection_ == NULL) {
            nabtoConnection_ = nabto_device_virtual_connection_new(device_->getDevice());

        }
        coapChannel_ = WebrtcCoapChannel::create(pc_, incoming, device_, nabtoConnection_);
    }
    else if (incoming->label().find("stream-") == 0) {
        std::cout << "Stream channel opened: " << incoming->label() << std::endl;
        std::cout << "Stream port: " << incoming->label().substr(7) << std::endl;
        try {
        uint32_t port = std::stoul(incoming->label().substr(7));
        std::cout << "Stream port: " << port << std::endl;

        if (nabtoConnection_ == NULL) {
            nabtoConnection_ = nabto_device_virtual_connection_new(device_->getDevice());

        }
        streamChannel_ = WebrtcFileStreamChannel::create(incoming, device_, nabtoConnection_, port);
        } catch (std::exception &e) {
            std::cout << "error " << e.what() << std::endl;
        }
    }

}


} // namespace
