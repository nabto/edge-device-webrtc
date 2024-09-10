#include "webrtc_connection.hpp"
#include "webrtc_util.hpp"

#include <signaling-stream/signaling_stream.hpp>
#include <api/media_track_impl.hpp>
#include <api/datachannel_impl.hpp>

#include <rtc/rtc.hpp>

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

void WebrtcConnection::handleCandidate(rtc::Candidate cand)
{
    NPLOGD << "Got ICE: " << cand;
    try {
        pc_->addRemoteCandidate(cand);
    } catch (std::logic_error& ex) {
        if (!this->ignoreOffer_) {
            NPLOGE << "Failed to add remote ICE candidate with logic error: " << ex.what();
        }
    }
}

void WebrtcConnection::handleDescription(rtc::Description desc)
{
    NPLOGD << "Got Description: " << desc;
    if (!pc_) {
        createPeerConnection();
    }
    try {
        // TODO: remove metadata from v2 but maybe keep for v1
        nlohmann::json metadata;
        handleSignalingMessage(desc, metadata);
    } catch (std::invalid_argument& ex) {
        NPLOGE << "Got invalid argument: " << ex.what();
    }
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

    pc_->onStateChange([self](const rtc::PeerConnection::State& state) {
        std::ostringstream oss;
        oss << state;
        NPLOGD << "State: " << oss.str();
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
            std::ostringstream oss;
            oss << state;
            NPLOGD << "Signalling State: " << oss.str();
            self->queue_->post([self, state]() {
                self->handleSignalingStateChange(state);
            });
        });

    pc_->onLocalCandidate([self](rtc::Candidate cand) {
        NPLOGD << "Got local candidate: " << cand;
        self->queue_->post([self, cand]() {
            if (self->canTrickle_) {
                self->updateMetaTracks();
                self->sigStream_->signalingSendCandidate(cand, self->metadata_);
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
            std::ostringstream oss;
            oss << state;
            NPLOGD << "Gathering State: " << oss.str();
            self->queue_->post([self, state]() {
                if (state == rtc::PeerConnection::GatheringState::Complete && !self->canTrickle_) {
                    auto description = self->pc_->localDescription();
                    NPLOGD << "Sending description: " << std::string(description.value());
                    self->updateMetaTracks();
                    self->sigStream_->signalingSendDescription(description.value(), self->metadata_);
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
        std::ostringstream oss;
        oss << state;
        NPLOGD << "Got unhandled signaling state: " << oss.str();
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
        trackCb_(getId(), track);
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
    // TODO: remove "coap" label when we are confident clients have been updated.
    if (incoming->label() == "coap" || incoming->label() == "nabto-coap") {
        if (nabtoConnection_ == NULL) {
            nabtoConnection_ = nabto_device_virtual_connection_new(device_.get());

        }
        coapChannel_ = WebrtcCoapChannel::create(pc_, incoming, device_, nabtoConnection_, queue_);
    }
    // TODO: remove "stream-" label when we are confident clients have been updated
    else if (incoming->label().find("stream-") == 0 || incoming->label().find("nabto-stream-") == 0) {
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
        datachannelCb_(getId(), chan);
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
    // TODO: fix metadata handling
    bool hasError = false;
    try {
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
    } catch (std::exception& ex) {
        NPLOGE << "METATRACK EXCEPTION: " << ex.what();
    }
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

        sigStream_->signalingSendDescription(description.value(), metadata_);

        if (description->type() == rtc::Description::Type::Answer) {
            if (pc_->negotiationNeeded()) {
                // We where polite and have finished resolving a collided offer, recreate our offer
                pc_->setLocalDescription();
            }
        }
    }
}

} // namespace
