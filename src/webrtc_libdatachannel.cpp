#include "webrtc_libdatachannel.hpp"
#include "webrtc_stream.hpp"

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace nabto {

void WebrtcChannel::createPeerConnection()
{
    auto self = shared_from_this();
    rtc::Configuration conf;

    // TODO: this can probably be removed since we do not do auth over datachannel as in the POC
    conf.forceMediaTransport = true;

    for (auto t : turnServers_) {
        conf.iceServers.push_back(rtc::IceServer(t.hostname, t.port, t.username, t.password));
    }

    // rtc::IceServer turn("3.252.194.140", 3478, "1675935678:foo", "D/9Nw9yGzXGL+qy/mvwLlXfOgVI=");
    //rtc::IceServer turn("127.0.0.1", 3478, "1675935678:foo","D/9Nw9yGzXGL+qy/mvwLlXfOgVI=");
    //rtc::IceServer turn("127.0.0.1", 3478, "foo", "bar123");
    // conf.iceTransportPolicy = rtc::TransportPolicy::Relay;

    conf.disableAutoNegotiation = true;

    pc_ = std::make_shared<rtc::PeerConnection>(conf);

    pc_->onStateChange([self](rtc::PeerConnection::State state) {
        std::cout << "State: " << state << std::endl;
        if (state == rtc::PeerConnection::State::Connected) {
            if (self->eventHandler_) {
                self->eventHandler_(ConnectionEvent::CONNECTED);
            }
        } else if (state == rtc::PeerConnection::State::Closed) {
            if (self->eventHandler_) {
                self->eventHandler_(ConnectionEvent::CLOSED);
            }
            self->eventHandler_ = nullptr;
            self->sendSignal_ = nullptr;
        }
        });

    pc_->onSignalingStateChange(
        [self](rtc::PeerConnection::SignalingState state) {
            std::cout << "Signalling State: " << state << std::endl;
            if (state ==
                rtc::PeerConnection::SignalingState::HaveLocalOffer) {
                auto description = self->pc_->localDescription();
                json message = {
                    {"type", description->typeString()},
                    {"sdp", std::string(description.value())} };
                auto type =
                    description->type() ==
                    rtc::Description::Type::Answer
                    ? WebrtcStream::ObjectType::WEBRTC_ANSWER
                    : WebrtcStream::ObjectType::WEBRTC_OFFER;
                json offer = {
                    {"type", type},
                    {"data", message}
                };
                std::cout << "SENDING " << type << " message: " << message << std::endl;
                auto data = offer.dump();
                self->sendSignal_(data);
            }
            if (state ==
                rtc::PeerConnection::SignalingState::HaveRemoteOffer) {
                try {
                    self->pc_->setLocalDescription();//(rtc::Description::Type::Answer);
                }
                catch (std::logic_error ex) {
                    std::cout << "EXCEPTION!!!! " << ex.what() << std::endl;
                }

                auto description = self->pc_->localDescription();
                std::cout << "local desc is type: " << description->typeString() << std::endl;
                json message = {
                    {"type", description->typeString()},
                    {"sdp", std::string(description.value())} };
                auto type =
                    description->type() ==
                    rtc::Description::Type::Answer
                    ? WebrtcStream::ObjectType::WEBRTC_ANSWER
                    : WebrtcStream::ObjectType::WEBRTC_OFFER;
                json offer = {
                    {"type", type},
                    {"data", message}
                };
                std::cout << "SENDING " << type << " message: " << message << std::endl;
                auto data = offer.dump();
                self->sendSignal_(data);
            }
        });

    pc_->onGatheringStateChange(
        [self](rtc::PeerConnection::GatheringState state) {
            std::cout << "Gathering State: " << state << std::endl;
            self->state_ = state;
            // if (state ==
            //     rtc::PeerConnection::GatheringState::Complete) {
            //     auto description = self->pc_->localDescription();
            //     json message = {
            //         {"type", description->typeString()},
            //         {"sdp", std::string(description.value())} };
            //     std::cout << message << std::endl;
            //     auto type =
            //         description->type() ==
            //         rtc::Description::Type::Answer
            //         ? WebrtcStream::ObjectType::WEBRTC_ANSWER
            //         : WebrtcStream::ObjectType::WEBRTC_OFFER;
            //     json offer = {
            //         {"type", type},
            //         {"data", message}
            //     };
            //     std::cout << "SENDING " << type << std::endl;
            //     auto data = offer.dump();
            //     self->sendSignal_(data);
            // }
        });

    pc_->onLocalCandidate([self](rtc::Candidate cand) {
        std::cout << "Got local candidate: " << cand << std::endl;
        json candidate;
        candidate["sdpMid"] = cand.mid();
        candidate["candidate"] = cand.candidate();
        json answer = { {"type", WebrtcStream::ObjectType::WEBRTC_ICE},
                        {"data", candidate} };
        if (true || self->state_ ==
            rtc::PeerConnection::GatheringState::Complete) {
            std::cout << "SENDING NEW-ICE-CANDIDATE" << std::endl;
            auto data = answer.dump();
            self->sendSignal_(data);
        }
        });

    pc_->onTrack([self](std::shared_ptr<rtc::Track> track) {
        std::cout << "Got Track event" << std::endl;
        try {
            auto media = track->description();
            rtc::Description::Media::RtpMap* rtp = media.rtpMap(108);
            media.addSSRC(self->ssrc_, "video-send");
            self->track_ = self->pc_->addTrack(media);
        } catch (std::exception ex) {
            std::cout << "GOT EXCEPTION!!! " << ex.what() << std::endl;
        }
    });
}


void WebrtcChannel::setupVideoDescription()
{
    // TODO: This function is unused
    // TODO: rtsp_.init();
    // TODO: more flexible codec and video ID
    rtc::Description::Video media("video", rtc::Description::Direction::SendOnly);
    media.addH264Codec(108); // Must match the payload type of the external h264 RTP stream
    media.addSSRC(ssrc_, "video-send");
    track_ = pc_->addTrack(media);

    pc_->setLocalDescription();
}


void WebrtcChannel::handleOffer(std::string& offer) {
    std::cout << "Got Offer: " << offer << std::endl;
    if (!pc_) {
        createPeerConnection();
    }
    // TODO: handle json exceptions
    json sdp = json::parse(offer);
    rtc::Description remDesc(sdp["sdp"].get<std::string>(), sdp["type"].get<std::string>());
    try {
    pc_->setRemoteDescription(remDesc);
    } catch (std::invalid_argument& ex) {
        std::cout << "GOT INVALID ARGUMENT: " << ex.what() << std::endl;
    }
}

void WebrtcChannel::handleAnswer(std::string& answer) {
    std::cout << "Got Answer: " << answer << std::endl;
    // TODO: handle json exceptions
    json sdp = json::parse(answer);
    rtc::Description remDesc(sdp["sdp"].get<std::string>(), sdp["type"].get<std::string>());
    pc_->setRemoteDescription(remDesc);
}

void WebrtcChannel::handleIce(std::string& ice) {
    std::cout << "Got ICE: " << ice << std::endl;
    //return;
    // TODO: handle json exceptions
    json candidate = json::parse(ice);
    rtc::Candidate cand(candidate["candidate"].get<std::string>(), candidate["sdpMid"].get<std::string>());
    pc_->addRemoteCandidate(cand);
}


} // Namespace
