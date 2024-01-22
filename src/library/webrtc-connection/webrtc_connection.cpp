#include "webrtc_connection.hpp"
#include "webrtc_util.hpp"

#include <signaling-stream/signaling_stream.hpp>
#include <api/media_track_impl.hpp>

#include <nlohmann/json.hpp>

#include <iostream>

namespace nabto {

WebrtcConnectionPtr WebrtcConnection::create(SignalingStreamPtr sigStream, NabtoDevicePtr device, std::vector<struct TurnServer>& turnServers, EventQueuePtr queue, TrackEventCallback trackCb, CheckAccessCallback accessCb)
{
    return std::make_shared<WebrtcConnection>(sigStream, device, turnServers, queue, trackCb, accessCb);
}

WebrtcConnection::WebrtcConnection(SignalingStreamPtr sigStream, NabtoDevicePtr device, std::vector<struct TurnServer>& turnServers, EventQueuePtr queue, TrackEventCallback trackCb, CheckAccessCallback accessCb)
    : sigStream_(sigStream), device_(device), turnServers_(turnServers), queue_(queue), trackCb_(trackCb), accessCb_(accessCb), queueWork_(queue)
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

void WebrtcConnection::handleOffer(std::string& data)
{
    std::cout << "Got Offer: " << data << std::endl;
    if (!pc_) {
        createPeerConnection();
    }
    try {
        nlohmann::json sdp = nlohmann::json::parse(data);
        rtc::Description remDesc(sdp["sdp"].get<std::string>(), sdp["type"].get<std::string>());

        // std::cout << "Setting remDesc: " << remDesc << std::endl;
        try {
            pc_->setRemoteDescription(remDesc);
        } catch (std::logic_error& ex) {
            std::cout << "Failed to set remote description with logic error: " << ex.what() << std::endl;
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
    } catch (std::logic_error& ex) {
        std::cout << "Failed to set remote description with logic error: " << ex.what() << std::endl;
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
    } catch (std::logic_error& ex) {
        std::cout << "Failed to add remote ICE candidate with logic error: " << ex.what() << std::endl;
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
                self->coapChannel_ = nullptr;
                self->pc_->close();
                self->pc_ = nullptr;
            }
        });
    });

    pc_->onSignalingStateChange(
        [self](rtc::PeerConnection::SignalingState state) {
            std::cout << "Signalling State: " << state << std::endl;
            self->queue_->post([self, state]() {
                self->handleSignalingStateChange(state);
            });
        });

    pc_->onLocalCandidate([self](rtc::Candidate cand) {
        std::cout << "Got local candidate: " << cand << std::endl;
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
        std::cout << "Got Track event" << std::endl;
        // Track events are triggered by setRemoteDescription, so they are already running on the event queue. This is likely the same for datachannels, however, tracks must be handled synchronously since they must remove all unacceptable codecs from the SDP.
        self->handleTrackEvent(track);
    });

    pc_->onDataChannel([self](std::shared_ptr<rtc::DataChannel> incoming) {
        std::cout << "Got new datachannel: " << incoming->label() << std::endl;
        self->queue_->post([self, incoming]() {
            self->handleDatachannelEvent(incoming);
        });
    });

    pc_->onGatheringStateChange(
        [self](rtc::PeerConnection::GatheringState state) {
            std::cout << "Gathering State: " << state << std::endl;
            self->queue_->post([self, state]() {
                if (state == rtc::PeerConnection::GatheringState::Complete && !self->canTrickle_) {
                    auto description = self->pc_->localDescription();
                    nlohmann::json message = {
                        {"type", description->typeString()},
                        {"sdp", std::string(description.value())} };
                    auto data = message.dump();
                    self->updateMetaTracks();
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
        updateMetaTracks();

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
    auto media = createMediaTrack(track);
    mediaTracks_.push_back(media);
    acceptTrack(media);
}

MediaTrackPtr WebrtcConnection::createMediaTrack(std::shared_ptr<rtc::Track> track)
{
    std::cout << "createMediaTrack metadata: " << metadata_.dump() << std::endl;
    auto mid = track->mid();
    auto sdp = track->description().generateSdp();
    try {
        auto metaTracks = metadata_["tracks"].get<std::vector<nlohmann::json>>();
        for (auto mt : metaTracks) {
            if (mt["mid"].get<std::string>() == mid) {
                std::cout << "Found metaTrack: " << mt.dump() << std::endl;
                auto trackId = mt["trackId"].get<std::string>();
                auto media = MediaTrack::create(trackId, sdp);
                media->getImpl()->setRtcTrack(track);
                return media;
            }
        }
    } catch (nlohmann::json::exception& ex) {
        std::cout << "createMediaTrack json exception: " << ex.what() << std::endl;
    } catch (std::runtime_error& ex) {
        std::cout << "createMediaTrack runtime error: " << ex.what() << std::endl;
    }
    std::string noTrack;
    auto media = MediaTrack::create(noTrack, sdp);
    return media;
}

void WebrtcConnection::acceptTrack(MediaTrackPtr track)
{
    if (trackCb_) {
        std::cout << "accepttrack with callback" << std::endl;
        trackCb_(getConnectionRef(), track);
        auto self = shared_from_this();
        track->getImpl()->getRtcTrack()->onMessage([self, track](rtc::message_variant data) {
            auto msg = rtc::make_message(data);
            self->queue_->post([self, track, msg]() {
                track->getImpl()->handleTrackMessage(msg);
            });
        });
    } else {
        std::cout << "acceptTrack WITHOUT CALLBACK" << std::endl;
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
        std::cout << "Stream channel opened: " << incoming->label() << std::endl;
        std::cout << "Stream port: " << incoming->label().substr(7) << std::endl;
        try {
        uint32_t port = std::stoul(incoming->label().substr(7));
        std::cout << "Stream port: " << port << std::endl;

        if (nabtoConnection_ == NULL) {
            nabtoConnection_ = nabto_device_virtual_connection_new(device_.get());

        }
        streamChannel_ = WebrtcFileStreamChannel::create(incoming, device_, nabtoConnection_, port, queue_);
        } catch (std::exception &e) {
            std::cout << "error " << e.what() << std::endl;
        }
    }

}

NabtoDeviceConnectionRef WebrtcConnection::getConnectionRef() {
    if (nabtoConnection_ != NULL) {
        return nabto_device_connection_get_connection_ref(nabtoConnection_);
    }
    else {
        return sigStream_->getSignalingConnectionRef();
    }
}

void WebrtcConnection::createTracks(std::vector<MediaTrackPtr>& tracks)
{
    if (!pc_) {
        createPeerConnection();
    }
    for (auto t : tracks) {
        auto sdp = t->getSdp();
        // TODO: remove when updating libdatachannel after https://github.com/paullouisageneau/libdatachannel/issues/1074
        if (sdp[0] == 'm' && sdp[1] == '=') {
            sdp = sdp.substr(2);
        }
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
    std::cout << "createTracks Set local description" << std::endl;
    pc_->setLocalDescription();
}

void WebrtcConnection::updateMetaTracks()
{
    bool hasError = false;
    for (auto m: mediaTracks_) {
        auto error = m->getImpl()->getErrorState();
        if (error != MediaTrack::ErrorState::OK) {
            hasError = true;
            auto sdp = m->getSdp();
            // TODO: remove when updating libdatachannel after https://github.com/paullouisageneau/libdatachannel/issues/1074
            if (sdp[0] == 'm' && sdp[1] == '=') {
                sdp = sdp.substr(2);
            }
            rtc::Description::Media media(sdp);
            auto mid = media.mid();
            auto metaTracks = metadata_["tracks"].get<std::vector<nlohmann::json>>();
            bool found = false;
            for (auto& mt : metaTracks) {
                if (mt["mid"].get<std::string>() == mid) {
                    // Found the entry, insert error
                    mt["error"] = trackErrorToString(error);
                    found = true;
                    break;
                }
            }
            if (!found) {
                // This track was not in metadata, so we must add it to return the error
                nlohmann::json metaTrack = {
                    {"mid", mid},
                    {"trackId", m->getTrackId()},
                    {"error", trackErrorToString(error)}
                };
                metaTracks.push_back(metaTrack);
            }
            metadata_["tracks"] = metaTracks;
        }
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

} // namespace
