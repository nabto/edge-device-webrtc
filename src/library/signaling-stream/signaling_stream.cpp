#include "signaling_stream.hpp"

#include <nlohmann/json.hpp>

namespace nabto {

SignalingStreamPtr SignalingStream::create(NabtoDevicePtr device, NabtoDeviceStream* stream, SignalingStreamManagerPtr manager, EventQueuePtr queue, TrackEventCallback trackCb, CheckAccessCallback accessCb, DatachannelEventCallback datachannelCb, MetadataEventCallback metadataCb, bool isV2)
{
    return std::make_shared<SignalingStream>(device, stream, manager, queue, trackCb, accessCb, datachannelCb, metadataCb, isV2);

}

SignalingStream::SignalingStream(NabtoDevicePtr device, NabtoDeviceStream* stream, SignalingStreamManagerPtr manager, EventQueuePtr queue, TrackEventCallback trackCb, CheckAccessCallback accessCb, DatachannelEventCallback datachannelCb, MetadataEventCallback metadataCb, bool isV2)
    :device_(device), stream_(stream), manager_(manager), queue_(queue), trackCb_(trackCb), datachannelCb_(datachannelCb), metadataCb_(metadataCb), accessCb_(accessCb), isV2_(isV2)
{
    future_ = nabto_device_future_new(device.get());
    writeFuture_ = nabto_device_future_new(device.get());
}

SignalingStream::~SignalingStream()
{
    nabto_device_stream_free(stream_);
    nabto_device_future_free(future_);
    nabto_device_future_free(writeFuture_);
}

void SignalingStream::start()
{
    // TODO: better identifier
    const char* identifier = "foobar";
    auto dev = device_.get();

    iceReq_ = nabto_device_ice_servers_request_new(dev);
    auto iceFut = nabto_device_future_new(dev);
    nabto_device_ice_servers_request_send(identifier, iceReq_, iceFut);
    nabto_device_future_set_callback(iceFut, iceServersResolved, this);

    nabto_device_stream_accept(stream_, future_);
    self_ = shared_from_this();
    nabto_device_future_set_callback(future_, streamAccepted, this);

}

void SignalingStream::streamAccepted(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData) {
    SignalingStream* self = (SignalingStream*)userData;
    if (ec != NABTO_DEVICE_EC_OK) {
        self->queue_->post([self]() {
            self->self_ = nullptr;
            if (self->webrtcConnection_) {
                self->webrtcConnection_->stop();
            }
        });
        return;
    }
    self->queue_->post([self]() {
        self->accepted_ = true;
        if (self->webrtcConnection_) {
            // If ice servers request returned first we start reading here
            NPLOGD << "Stream accepted after ICE servers. Start reading";
            self->readObjLength();
        }
    });
}

void SignalingStream::iceServersResolved(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData) {
    SignalingStream* self = (SignalingStream*)userData;
    nabto_device_future_free(future);
    self->queue_->post([self, ec]() {
        if (ec == NABTO_DEVICE_EC_OK) {
            self->parseIceServers();
            NPLOGD << "Got ICE servers, creating channel";
        } else {
            NPLOGE << "Failed to get ICE servers. Continuing without TURN";
        }
        self->createWebrtcConnection();
        if (self->accepted_) {
            // If accepted returned first we start reading here
            NPLOGD << "Ice servers after stream accept. Start reading";
            self->readObjLength();
        }
        nabto_device_ice_servers_request_free(self->iceReq_);
    });
}

void SignalingStream::parseIceServers() {
    size_t n = nabto_device_ice_servers_request_get_server_count(iceReq_);
    for (size_t i = 0; i < n; i++) {
        const char* username = nabto_device_ice_servers_request_get_username(iceReq_, i);
        const char* cred = nabto_device_ice_servers_request_get_credential(iceReq_, i);
        size_t urlCount = nabto_device_ice_servers_request_get_urls_count(iceReq_, i);
        WebrtcConnection::TurnServer turn;
        memset(&turn, 0, sizeof(turn));
        if (username != NULL) {
            turn.username = std::string(username);
        }
        if (cred != NULL) {
            turn.credential = std::string(cred);
        }

        for (size_t u = 0; u < urlCount; u++) {
            std::string url = std::string(nabto_device_ice_servers_request_get_url(iceReq_, i, u));
            turn.urls.push_back(url);
        }
        turnServers_.push_back(turn);
    }
}

void SignalingStream::createWebrtcConnection() {
    auto self = shared_from_this();
    webrtcConnection_ = WebrtcConnection::create(self, device_, turnServers_, queue_, trackCb_, accessCb_, datachannelCb_);
    webrtcConnection_->setEventHandler([self](WebrtcConnection::ConnectionState state) {
        if (state == WebrtcConnection::ConnectionState::CLOSED ||
            state == WebrtcConnection::ConnectionState::FAILED) {
                self->closeStream();
        }
    });
    if (!deferredTracks_.empty()) {
        createTracks(deferredTracks_);
        deferredTracks_.clear();
    }
}

void SignalingStream::sendSignalligObject(const std::string& data)
{
    NPLOGD << "Sending signaling object " << data;
    writeBuffers_.push(data);

    tryWriteStream();
}

void SignalingStream::tryWriteStream()
{
    std::string data;
    if (closed_) {
        closeStream();
        return;
    }
    if (writeBuf_ != NULL) {
        NPLOGD << "Write while writing";
        return;
    }

    if (writeBuffers_.empty()) {
        return;
    }
    data = writeBuffers_.front();
    writeBuffers_.pop();

    uint32_t size = data.size();
    writeBuf_ = (uint8_t*)calloc(1, size + 4);
    memcpy(writeBuf_, &size, 4);
    memcpy(writeBuf_ + 4, data.data(), size);

    nabto_device_stream_write(stream_, writeFuture_, writeBuf_, size + 4);
    nabto_device_future_set_callback(writeFuture_, streamWriteCallback, this);
}

void SignalingStream::streamWriteCallback(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    (void)ec;
    SignalingStream* self = (SignalingStream*)userData;
    self->queue_->post([self]() {
        free(self->writeBuf_);
        self->writeBuf_ = NULL;
        self->tryWriteStream();
    });
}


void SignalingStream::readObjLength()
{
    if (closed_) {
        NPLOGD << "Read after closed. Self destruct";
        return cleanup();
    }
    reading_ = true;
    objectLength_ = 0;
    // NabtoDeviceFuture* readFuture = nabto_device_future_new(device_->getDevice());
    nabto_device_stream_read_all(stream_, future_, &objectLength_, 4, &readLength_);
    nabto_device_future_set_callback(future_, hasReadObjLen, this);
}

void SignalingStream::hasReadObjLen(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    SignalingStream* self = (SignalingStream*)userData;
    // nabto_device_future_free(future);
    if (ec == NABTO_DEVICE_EC_EOF) {
        // make a nice shutdown
        NPLOGD << "Read reached EOF closing nicely";
        self->queue_->post([self]() {
            self->reading_ = false;
            self->closeStream();
        });
        return;
    }
    if (ec != NABTO_DEVICE_EC_OK) {
        NPLOGI << "Read failed with " << nabto_device_error_get_message(ec) << " cleaning up";
        self->queue_->post([self]() {
            self->reading_ = false;
            self->cleanup();
        });
        return;
    }
    self->queue_->post([self]() {
        self->reading_ = false;
        self->handleReadObjLen();
    });
}

void SignalingStream::handleReadObjLen()
{
    if (readLength_ < 4 || objectLength_ < 1) {
        NPLOGE << "Bad Length read: " << objectLength_;
        return readObjLength();
    }
    return readObject(objectLength_);
}

void SignalingStream::readObject(uint32_t len)
{
    if (closed_) {
        NPLOGD << "Read after closed. Self destruct";
        return cleanup();
    }
    reading_ = true;
    objectBuffer_ = (uint8_t*)calloc(1, len);
    // NabtoDeviceFuture* readFuture = nabto_device_future_new(device_->getDevice());
    nabto_device_stream_read_all(stream_, future_, objectBuffer_, len, &readLength_);
    nabto_device_future_set_callback(future_, hasReadObject, this);

}

void SignalingStream::hasReadObject(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    SignalingStream* self = (SignalingStream*)userData;
    if (ec == NABTO_DEVICE_EC_EOF) {
        // make a nice shutdown
        NPLOGD << "Read reached EOF closing nicely";
        self->queue_->post([self]() {
            self->reading_ = false;
            self->closeStream();
        });
        return;
    }
    if (ec != NABTO_DEVICE_EC_OK) {
        NPLOGE << "Read failed with " << nabto_device_error_get_message(ec) << " cleaning up";
        self->queue_->post([self]() {
            self->reading_ = false;
            self->cleanup();
        });
        return;
    }
    self->queue_->post([self]() {
        self->reading_ = false;
        self->handleReadObject();
    });

}

void SignalingStream::handleReadObject()
{
    if (readLength_ < objectLength_ || webrtcConnection_ == nullptr) {
        // either we did not get all the data we wanted which means the stream is closing down or we stopped the webrtc connection due to an error. We just read object length again to get the error code and clean up.
        return readObjLength();
    }

    nlohmann::json obj;
    try {
        obj = nlohmann::json::parse(objectBuffer_, objectBuffer_ + objectLength_);

        if (isV2_) {
            std::string type = obj["type"].get<std::string>();
            if (type == "SETUP_REQUEST") {
                sendSetupResponse(obj);
            } else if (type == "CANDIDATE") {
                rtc::Candidate cand(obj["candidate"]["candidate"].get<std::string>(), obj["candidate"]["sdpMid"].get<std::string>());
                webrtcConnection_->handleCandidate(cand);
            } else if (type == "DESCRIPTION") {
                rtc::Description desc(obj["description"]["sdp"].get<std::string>(), obj["description"]["type"].get<std::string>());
                webrtcConnection_->handleDescription(desc);
            } else if (type == "METADATA") {
                if (metadataCb_) {
                    std::string metadata = obj["metadata"].get<std::string>();
                    metadataCb_(webrtcConnection_->getId(), metadata);
                } else {
                    NPLOGD << "Received metadata without callback";
                }
            } else {
                NPLOGE << "Received invalid signaling message: " << type;
            }
        } else {
            enum ObjectType type = static_cast<enum ObjectType>(obj["type"].get<int>());
            if (type == WEBRTC_OFFER || type == WEBRTC_ANSWER) {
                auto offer = obj["data"].get<std::string>();
                nlohmann::json metadata = obj["metadata"];
                webrtcConnection_->handleOfferAnswer(offer, metadata);
            } else if (type == WEBRTC_ICE) {
                auto ice = obj["data"].get<std::string>();
                webrtcConnection_->handleIce(ice);
            }
            else if (type == TURN_REQUEST) {
                sendTurnServers();
            }
            else {
                NPLOGE << "Unknown object type: " << type;
            }
        }
    }
    catch (nlohmann::json::parse_error& ex) {
        NPLOGE << "Failed to parse JSON: " << ex.what();
        NPLOGE << "parsing: " << std::string((char*)objectBuffer_, objectLength_);
        free(objectBuffer_);
        objectBuffer_ = NULL;
        return readObjLength();
    }
    free(objectBuffer_);
    objectBuffer_ = NULL;
    return readObjLength();
}

void SignalingStream::signalingSendMetadata(const std::string metadata)
{
    nlohmann::json msg = {
        {"type", "METADATA"},
        {"metadata", metadata}
    };
    auto data = msg.dump();
    sendSignalligObject(data);
}

void SignalingStream::signalingSendDescription(const rtc::Description& desc, const nlohmann::json& metadata)
{
    if (isV2_) {
        nlohmann::json description = {
            {"type", desc.typeString()},
            {"sdp", std::string(desc)}
        };
        nlohmann::json message = {
            {"type", "DESCRIPTION"},
            {"description", description},
            {"metadata", metadata}
        };
        auto obj = message.dump();
        sendSignalligObject(obj);
    } else {
        nlohmann::json message = {
            {"type", desc.typeString()},
            {"sdp", std::string(desc)}
        };
        auto data = message.dump();
        if (desc.type() == rtc::Description::Type::Offer)
        {
            signalingSendOffer(data, metadata);
        } else if (desc.type() == rtc::Description::Type::Answer) {
            signalingSendAnswer(data, metadata);
        }

    }
}

void SignalingStream::signalingSendOffer(const std::string& data, const nlohmann::json& metadata)
{
    nlohmann::json msg = {
         {"type", WEBRTC_OFFER},
         {"data", data},
         {"metadata", metadata}
    };
    auto obj = msg.dump();
    sendSignalligObject(obj);
}

void SignalingStream::signalingSendAnswer(const std::string& data, const nlohmann::json& metadata)
{
    nlohmann::json msg = {
         {"type", WEBRTC_ANSWER},
         {"data", data},
         {"metadata", metadata}
    };
    auto obj = msg.dump();
    sendSignalligObject(obj);

}

void SignalingStream::signalingSendCandidate(const rtc::Candidate& cand, nlohmann::json& metadata)
{
    if (isV2_) {
        nlohmann::json candidate = {
            {"candidate", cand.candidate()},
            {"sdpMid", cand.mid()}
        };
        nlohmann::json message = {
            {"type", "CANDIDATE"},
            {"candidate", candidate}
        };
        auto obj = message.dump();
        sendSignalligObject(obj);
    } else {
        nlohmann::json candidate;
        candidate["sdpMid"] = cand.mid();
        candidate["candidate"] = cand.candidate();
        auto data = candidate.dump();
        signalingSendIce(data, metadata);
    }
}

void SignalingStream::signalingSendIce(const std::string& data, const nlohmann::json& metadata)
{
    nlohmann::json msg = {
         {"type", WEBRTC_ICE},
         {"data", data},
         {"metadata", metadata}
    };
    auto obj = msg.dump();
    sendSignalligObject(obj);
}

void SignalingStream::sendSetupResponse(nlohmann::json req)
{
    try {
        bool polite = req["polite"].get<bool>();
        webrtcConnection_->setPolite(!polite);
    }catch(std::exception& ex) {
        // ignore
    }

    nlohmann::json resp = {
        {"type", "SETUP_RESPONSE"},
        {"polite", !webrtcConnection_->getPolite()},
        {"id", webrtcConnection_->getId()},
        {"iceServers", nlohmann::json::array()}
    };
    for (auto t : turnServers_) {
        nlohmann::json ice = {
            {"urls", nlohmann::json::array()}
        };
        if (!t.username.empty()) {
            ice["username"] = t.username;
        }
        if (!t.credential.empty()) {
            ice["credential"] = t.credential;
        }
        for (auto u : t.urls) {
            ice["urls"].push_back(u);
        }
        resp["iceServers"].push_back(ice);
    }
    auto str = resp.dump();
    sendSignalligObject(str);
}

void SignalingStream::sendTurnServers()
{
    nlohmann::json resp = {
        {"type", ObjectType::TURN_RESPONSE},
        {"servers", nlohmann::json::array()},
        {"iceServers", nlohmann::json::array()}
    };
    for (auto t : turnServers_) {
        // TODO: remove this deprecated field in the future
        if (t.urls.size() > 0){
            nlohmann::json turn = {
                {"hostname", t.urls[0]},
                {"port", 0},
                {"username", t.username},
                {"password", t.credential}
            };
            resp["servers"].push_back(turn);
        }

        nlohmann::json ice = {
            {"urls", nlohmann::json::array()}
        };
        if (!t.username.empty()) {
            ice["username"] = t.username;
        }
        if (!t.credential.empty()) {
            ice["credential"] = t.credential;
        }
        for (auto u : t.urls) {
            ice["urls"].push_back(u);
        }
        resp["iceServers"].push_back(ice);
    }
    auto str = resp.dump();
    sendSignalligObject(str);
}


void SignalingStream::closeStream()
{
    if (closed_) {
        if (!closing_ && !reading_ && writeBuf_ == NULL) {
            NPLOGD << "Got close when closed. Not reading not writing not closing. cleaning up";
            cleanup();
        }
        return;
    }
    closed_ = true;
    closing_ = true;
    if (webrtcConnection_ != nullptr) {
        webrtcConnection_->stop();
        webrtcConnection_ = nullptr;
    }
    NabtoDeviceFuture* closeFuture = nabto_device_future_new(device_.get());
    nabto_device_stream_close(stream_, closeFuture);
    nabto_device_future_set_callback(closeFuture, streamClosed, this);
}

void SignalingStream::streamClosed(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    NPLOGD << "stream closed";
    (void)ec;
    nabto_device_future_free(future);
    SignalingStream* self = (SignalingStream*)userData;
    self->queue_->post([self]() {
        self->closing_ = false;
        if (!self->reading_ && self->writeBuf_ == NULL) {
            NPLOGD << "Not reading && writeBuf is NULL";
            self->cleanup();
        } else {
            NPLOGD << "reading or writing on closed. Awaiting self destuct";
        }
    });
}

void SignalingStream::cleanup()
{
    closed_ = true;
    if (webrtcConnection_ != nullptr) {
        webrtcConnection_->stop();
        webrtcConnection_ = nullptr;
    }

    webrtcConnection_ = nullptr;
    self_ = nullptr;
}


} // namespace
