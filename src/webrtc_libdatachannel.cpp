#include "webrtc_libdatachannel.hpp"
#include "webrtc_stream.hpp"

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace nabto {

void WebrtcChannel::createPeerConnection()
{
    auto self = shared_from_this();
    rtc::Configuration conf;

    for (auto t : turnServers_) {
        conf.iceServers.push_back(rtc::IceServer(t.hostname, t.port, t.username, t.password));
    }

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
            rtc::Description::Media::RtpMap* rtp = NULL;
            for (auto pt : media.payloadTypes()) {
                rtc::Description::Media::RtpMap* r = NULL;
                try {
                    r = media.rtpMap(pt);
                } catch (std::exception& ex) {
                    std::cout << "Bad rtpMap for pt: " << pt << std::endl;
                    continue;
                }
                // TODO: make codec configureable and generalize this matching
                // std::string profLvlId = "42e01f";
                // std::string lvlAsymAllowed = "1";
                // std::string pktMode = "1";
                std::string profLvlId = "4d001f";
                std::string lvlAsymAllowed = "1";
                std::string pktMode = "1";
                if (r != NULL && r->fmtps.size() > 0 &&
                    r->fmtps[0].find("profile-level-id="+profLvlId) != std::string::npos &&
                    r->fmtps[0].find("level-asymmetry-allowed="+lvlAsymAllowed) != std::string::npos &&
                    r->fmtps[0].find("packetization-mode="+pktMode) != std::string::npos
                    ) {
                    std::cout << "FOUND RTP codec match!!! " << pt << std::endl;
                    rtp = r;
                } else {
                    std::cout << "no match, removing" << std::endl;
                    media.removeRtpMap(pt);
                }
            }
            // TODO: handle no match found error


            // rtc::Description::Media::RtpMap* rtp = media.rtpMap(108);
            // std::cout << "RTP format: " << rtp->format << std::endl
            // << "RTP encParams: " << rtp->encParams << std::endl
            //     << "RTP clockRate: " << rtp->clockRate << std::endl;
            // std::cout << "rtcpFbs: " << std::endl;
            // for (auto fb : rtp->rtcpFbs) {
            //     std::cout << "   " << fb << std::endl;
            // }
            // std::cout << "fmtp: " << std::endl;
            // for (auto fb : rtp->fmtps) {
            //     std::cout << "   " << fb << std::endl;
            // }
            // TODO: not-hardcoded srcPayloadType
            self->srcPayloadType_ = 96;
            self->dstPayloadType_ = rtp->payloadType;
            media.addSSRC(self->ssrc_, "video-send");
            self->track_ = self->pc_->addTrack(media);
        } catch (std::exception ex) {
            std::cout << "GOT EXCEPTION!!! " << ex.what() << std::endl;
        }
    });
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
