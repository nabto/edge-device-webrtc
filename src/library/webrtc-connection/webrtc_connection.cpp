#include "webrtc_connection.hpp"
#include "webrtc_util.hpp"

#include <signaling-stream/signaling_stream.hpp>
#include <api/media_track_impl.hpp>
#include <api/datachannel_impl.hpp>

#include <nlohmann/json.hpp>

#include <iostream>

namespace nabto {

WebrtcConnectionPtr WebrtcConnection::create(SignalingStreamPtr sigStream, NabtoDevicePtr device, std::vector<struct TurnServer>& turnServers, EventQueuePtr queue, TrackEventCallback trackCb, CheckAccessCallback accessCb, DatachannelEventCallback datachannelCb)
{
    return std::make_shared<WebrtcConnection>(sigStream, device, turnServers, queue, trackCb, accessCb, datachannelCb);
}

WebrtcConnection::WebrtcConnection(SignalingStreamPtr sigStream, NabtoDevicePtr device, std::vector<struct TurnServer>& turnServers, EventQueuePtr queue, TrackEventCallback trackCb, CheckAccessCallback accessCb, DatachannelEventCallback datachannelCb)
    : sigStream_(sigStream), device_(device), turnServers_(turnServers), queue_(queue), trackCb_(trackCb), datachannelCb_(datachannelCb), accessCb_(accessCb), queueWork_(queue)
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
    if (pc_) {
        pc_->close();
    }
    // sigStream_ = nullptr;
}

void WebrtcConnection::handleOfferAnswer(const std::string& data, const nlohmann::json& metadata) {
    NPLOGD << "Got Offer/Answer: " << data;
     if (!pc_) {
        createPeerConnection();
    }
    try {
        nlohmann::json sdp = nlohmann::json::parse(data);
        rtc::Description remDesc(sdp["sdp"].get<std::string>(), sdp["type"].get<std::string>());

        handleSignalingMessage(remDesc, metadata);
    } catch (std::invalid_argument& ex) {
        NPLOGE << "Got invalid argument: " << ex.what();
    }
    catch (nlohmann::json::exception& ex) {
        NPLOGE << "handleOffer json exception: " << ex.what();
    }

}

void WebrtcConnection::handleIce(const std::string& data)
{
    NPLOGD << "Got ICE: " << data;
    try {
        nlohmann::json candidate = nlohmann::json::parse(data);
        rtc::Candidate cand(candidate["candidate"].get<std::string>(), candidate["sdpMid"].get<std::string>());
        pc_->addRemoteCandidate(cand);
    }
    catch (nlohmann::json::exception& ex) {
        NPLOGE << "handleIce json exception: " << ex.what();
    } catch (std::logic_error& ex) {
        if (!this->ignoreOffer_) {
           NPLOGE << "Failed to add remote ICE candidate with logic error: " << ex.what();
        }
    }
}

void WebrtcConnection::createPeerConnection()
{
    auto self = shared_from_this();
    rtc::Configuration conf;

    if (!webrtc_util::parseTurnServers(conf, turnServers_)) {
        NPLOGE << "Failed to parce TURN server configurations";
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
        NPLOGD << "State: " << state;
        self->queue_->post([self, state]() {
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
                for (auto m : self->mediaTracks_) {
                    m->getImpl()->connectionClosed();
                }

                for (auto d : self->datachannels_) {
                    d->getImpl()->connectionClosed();
                }
                self->datachannels_.clear();

                self->coapChannel_ = nullptr;
                self->pc_->close();
                self->pc_ = nullptr;
            }
        });
    });

    pc_->onSignalingStateChange(
        [self](rtc::PeerConnection::SignalingState state) {
            NPLOGD << "Signalling State: " << state;
            self->queue_->post([self, state]() {
                self->handleSignalingStateChange(state);
            });
        });

    pc_->onLocalCandidate([self](rtc::Candidate cand) {
        NPLOGD << "Got local candidate: " << cand;
        self->queue_->post([self, cand]() {
            if (self->canTrickle_) {
                nlohmann::json candidate;
                candidate["sdpMid"] = cand.mid();
                candidate["candidate"] = cand.candidate();
                auto data = candidate.dump();
                self->updateMetaTracks();
                self->sigStream_->signalingSendIce(data, self->metadata_);
            }
        });
    });

    pc_->onTrack([self](std::shared_ptr<rtc::Track> track) {
        NPLOGD << "Got Track event";
        // Track events are triggered by setRemoteDescription, so they are already running on the event queue. This is likely the same for datachannels, however, tracks must be handled synchronously since they must remove all unacceptable codecs from the SDP.
        self->handleTrackEvent(track);
    });

    pc_->onDataChannel([self](std::shared_ptr<rtc::DataChannel> incoming) {
        NPLOGD << "Got new datachannel: " << incoming->label();
        // Datachannel events must also be handled synchronously, otherwise messages sent by the client immidiately after creation may be lost
        self->handleDatachannelEvent(incoming);
    });

    pc_->onGatheringStateChange(
        [self](rtc::PeerConnection::GatheringState state) {
            NPLOGD << "Gathering State: " << state;
            self->queue_->post([self, state]() {
                if (state == rtc::PeerConnection::GatheringState::Complete && !self->canTrickle_) {
                    auto description = self->pc_->localDescription();
                    nlohmann::json message = {
                        {"type", description->typeString()},
                        {"sdp", std::string(description.value())} };
                    auto data = message.dump();
                    self->updateMetaTracks();
                    if (description->type() == rtc::Description::Type::Offer) {
                        NPLOGD << "Sending offer: " << std::string(description.value());
                        self->sigStream_->signalingSendOffer(data, self->metadata_);
                    }
                    else {
                        NPLOGD << "Sending answer: " << std::string(description.value());
                        self->sigStream_->signalingSendAnswer(data, self->metadata_);
                    }
                }
            });
        });

}

void WebrtcConnection::handleSignalingStateChange(rtc::PeerConnection::SignalingState state)
{
    if (state ==
        rtc::PeerConnection::SignalingState::HaveLocalOffer) {
    } else if (state == rtc::PeerConnection::SignalingState::HaveRemoteOffer) {
    } else {
        NPLOGD << "Got unhandled signaling state: " << state;
        return;
    }
    if (canTrickle_ || pc_->gatheringState() == rtc::PeerConnection::GatheringState::Complete) {
        auto description = pc_->localDescription();
        sendDescription(description);
    }

}

void WebrtcConnection::handleTrackEvent(std::shared_ptr<rtc::Track> track)
{
    NPLOGD << "Track event metadata: " << metadata_.dump();
    auto media = createMediaTrack(track);
    mediaTracks_.push_back(media);
    acceptTrack(media);
}

MediaTrackPtr WebrtcConnection::createMediaTrack(std::shared_ptr<rtc::Track> track)
{
    NPLOGD << "createMediaTrack metadata: " << metadata_.dump();
    auto mid = track->mid();
    auto sdp = track->description().generateSdp();
    try {
        auto metaTracks = metadata_["tracks"].get<std::vector<nlohmann::json>>();
        for (auto mt : metaTracks) {
            if (mt["mid"].get<std::string>() == mid) {
                NPLOGD << "Found metaTrack: " << mt.dump();
                auto trackId = mt["trackId"].get<std::string>();
                auto media = MediaTrack::create(trackId, sdp);
                media->getImpl()->setRtcTrack(track);
                return media;
            }
        }
    } catch (nlohmann::json::exception& ex) {
        NPLOGE << "createMediaTrack json exception: " << ex.what();
    } catch (std::runtime_error& ex) {
        NPLOGE << "createMediaTrack runtime error: " << ex.what();
    }
    std::string noTrack;
    auto media = MediaTrack::create(noTrack, sdp);
    return media;
}

void WebrtcConnection::acceptTrack(MediaTrackPtr track)
{
    if (trackCb_) {
        NPLOGD << "accepttrack with callback";
        trackCb_(getConnectionRef(), track);
        if (track->getImpl()->getErrorState() != MediaTrack::ErrorState::OK) {
            NPLOGD << "track callback set a track error, not listening for messages";
            return;
        }
        auto self = shared_from_this();
        track->getImpl()->getRtcTrack()->onMessage([self, track](rtc::message_variant data) {
            auto msg = rtc::make_message(data);
            self->queue_->post([self, track, msg]() {
                track->getImpl()->handleTrackMessage(msg);
            });
        });
    } else {
        NPLOGE << "acceptTrack without callback";
        track->setErrorState(MediaTrack::ErrorState::UNKNOWN_ERROR);
    }
}

void WebrtcConnection::handleDatachannelEvent(std::shared_ptr<rtc::DataChannel> incoming)
{
    if (incoming->label() == "coap") {
        if (nabtoConnection_ == NULL) {
            nabtoConnection_ = nabto_device_virtual_connection_new(device_.get());

        }
        coapChannel_ = WebrtcCoapChannel::create(pc_, incoming, device_, nabtoConnection_, queue_);
    }
    else if (incoming->label().find("stream-") == 0) {
        NPLOGD << "Stream channel opened: " << incoming->label();
        NPLOGD << "Stream port: " << incoming->label().substr(7);
        try {
        uint32_t port = std::stoul(incoming->label().substr(7));
        NPLOGD << "Stream port: " << port;

        if (nabtoConnection_ == NULL) {
            nabtoConnection_ = nabto_device_virtual_connection_new(device_.get());

        }
        streamChannel_ = WebrtcFileStreamChannel::create(incoming, device_, nabtoConnection_, port, queue_);
        } catch (std::exception &e) {
            NPLOGE << "error " << e.what();
        }
    } else {
        auto chan = createDatachannel(incoming);
        datachannels_.push_back(chan);
        datachannelCb_(getConnectionRef(), chan);
    }

}

DatachannelPtr WebrtcConnection::createDatachannel(std::shared_ptr<rtc::DataChannel> channel)
{
    auto chan = Datachannel::create(channel->label());
    chan->getImpl()->setRtcChannel(channel);
    return chan;
}

NabtoDeviceConnectionRef WebrtcConnection::getConnectionRef() {
    if (nabtoConnection_ != NULL) {
        return nabto_device_connection_get_connection_ref(nabtoConnection_);
    }
    else {
        return sigStream_->getSignalingConnectionRef();
    }
}

void WebrtcConnection::createTracks(const std::vector<MediaTrackPtr>& tracks)
{
    if (!pc_) {
        createPeerConnection();
    }
    for (auto t : tracks) {
        auto sdp = t->getSdp();
        rtc::Description::Media media(sdp);
        nlohmann::json metaTrack = {
            {"mid", media.mid()},
            {"trackId", t->getTrackId()}
            };
        metadata_["tracks"].push_back(metaTrack);
        auto track = pc_->addTrack(media);
        t->getImpl()->setRtcTrack(track);
        mediaTracks_.push_back(t);

        auto self = shared_from_this();
        track->onMessage([self, t](rtc::message_variant data) {
            auto msg = rtc::make_message(data);
            self->queue_->post([self, t, msg]() {
                t->getImpl()->handleTrackMessage(msg);
                });
            });
    }
    NPLOGD << "createTracks Set local description";
    pc_->setLocalDescription();
}

void WebrtcConnection::updateMetaTracks() {
    bool hasError = false;
    for (auto m : mediaTracks_) {
        auto error = m->getImpl()->getErrorState();

        auto sdp = m->getSdp();
        rtc::Description::Media media(sdp);
        auto mid = media.mid();
        bool found = false;
        std::vector<nlohmann::json> metaTracks;
        try {
            metaTracks = metadata_["tracks"].get<std::vector<nlohmann::json>>();
            for (auto& mt : metaTracks) {
                if (mt["mid"].get<std::string>() == mid) {
                    // Found the entry, insert error
                    if (error != MediaTrack::ErrorState::OK) {
                        hasError = true;
                        mt["error"] = trackErrorToString(error);
                    }
                    found = true;
                    break;
                }
            }
        } catch (nlohmann::json::exception& ex) {
            NPLOGE << "Update metadata json exception: " << ex.what();
            continue;
        }
        if (!found) {
            // This track was not in metadata, so we must add it to return the
            // error
            nlohmann::json metaTrack = {{"mid", mid}, {"trackId", m->getTrackId()}};

            if (error != MediaTrack::ErrorState::OK) {
                metaTrack["error"] = trackErrorToString(error);
            }
            metaTracks.push_back(metaTrack);
        }
        metadata_["tracks"] = metaTracks;
    }

    metadata_["status"] = hasError ? "FAILED" : "OK";
}

std::string WebrtcConnection::trackErrorToString(enum MediaTrack::ErrorState state) {
    switch (state) {
    case MediaTrack::ErrorState::OK: return std::string("OK");
    case MediaTrack::ErrorState::ACCESS_DENIED: return std::string("ACCESS_DENIED");
    case MediaTrack::ErrorState::UNKNOWN_TRACK_ID: return std::string("UNKNOWN_TRACK_ID");
    case MediaTrack::ErrorState::INVALID_CODECS: return std::string("INVALID_CODECS");
    case MediaTrack::ErrorState::UNKNOWN_ERROR: return std::string("UNKNOWN_ERROR");
    }
    return "UNKNOWN_ERROR";
}

void WebrtcConnection::handleSignalingMessage(rtc::optional<rtc::Description> description, const nlohmann::json& metadata)
{
    try {
      if (description) {
        bool offerCollision =
          description->type() == rtc::Description::Type::Offer &&
          (makingOffer_ || pc_->signalingState() != rtc::PeerConnection::SignalingState::Stable);

        ignoreOffer_ = !polite_ && offerCollision;
        if (ignoreOffer_) {
            NPLOGD << "The device is impolite and there is a collision so we are discarding the received offer";
            return;
        }

        setMetadata(metadata);
        pc_->setRemoteDescription(*description);
        if (description->type() == rtc::Description::Type::Offer) {
          pc_->setLocalDescription();
        }
      }
    } catch (std::exception& err) {
        NPLOGE << "Failed to handle signaling message:" << err.what();
    }
}

void WebrtcConnection::sendDescription(rtc::optional<rtc::Description> description) {
    //this.consumePendingMetadata();
    if (description)
    {
        updateMetaTracks();
        nlohmann::json message = {
            {"type", description->typeString()},
            {"sdp", std::string(description.value())}};
        auto data = message.dump();
        if (description->type() == rtc::Description::Type::Offer)
        {
            sigStream_->signalingSendOffer(data, metadata_);
        }
        else if (description->type() == rtc::Description::Type::Answer)
        {
            sigStream_->signalingSendAnswer(data, metadata_);
        }
        else
        {
            NPLOGE << "Something happened which should not happen, please debug the code.";
        }
    }
}

} // namespace
