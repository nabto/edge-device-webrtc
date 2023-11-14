#include "signaling_stream.hpp"

#include <nlohmann/json.hpp>

namespace nabto {

SignalingStreamPtr SignalingStream::create(NabtoDeviceImplPtr device, NabtoDeviceStream* stream, SignalingStreamManagerPtr manager, std::vector<nabto::MediaStreamPtr>& medias, EventQueuePtr queue)
{
    return std::make_shared<SignalingStream>(device, stream, manager, medias, queue);

}

SignalingStream::SignalingStream(NabtoDeviceImplPtr device, NabtoDeviceStream* stream, SignalingStreamManagerPtr manager, std::vector<nabto::MediaStreamPtr>& medias, EventQueuePtr queue)
    :device_(device), stream_(stream), manager_(manager), medias_(medias), queue_(queue)
{
    future_ = nabto_device_future_new(device->getDevice());
    writeFuture_ = nabto_device_future_new(device->getDevice());
}

SignalingStream::~SignalingStream()
{
    std::cout << "SignalingStream Destructor" << std::endl;
    nabto_device_stream_free(stream_);
    nabto_device_future_free(future_);
    nabto_device_future_free(writeFuture_);
}

void SignalingStream::start()
{
    // TODO: better identifier
    const char* identifier = "foobar";
    auto dev = device_->getDevice();

    // TODO: maybe request new turn servers for the client when it asks instead of reusing these
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
            std::cout << "Stream accepted after ICE servers. Start reading" << std::endl;
            self->readObjLength();
        }
    });
}

void SignalingStream::iceServersResolved(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData) {
    SignalingStream* self = (SignalingStream*)userData;
    nabto_device_future_free(future);
    if (ec != NABTO_DEVICE_EC_OK && ec != NABTO_DEVICE_EC_NOT_ATTACHED) {
        self->queue_->post([self]() {
            if (self->webrtcConnection_ != nullptr) {
                self->webrtcConnection_->stop();
            }
            self->webrtcConnection_ = nullptr;
            self->self_ = nullptr;
            nabto_device_ice_servers_request_free(self->iceReq_);
        });
        return;
    }
    self->queue_->post([self, ec]() {
        if (ec == NABTO_DEVICE_EC_OK) {
            self->parseIceServers();
            std::cout << "Got ICE servers, creating channel" << std::endl;
        } else {
            std::cout << "Failed to get ICE servers. Continuing without TURN" << std::endl;
        }
        self->createWebrtcConnection();
        if (self->accepted_) {
            // If accepted returned first we start reading here
            std::cout << "Ice servers after stream accept. Start reading" << std::endl;
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

        for (size_t u = 0; u < urlCount; u++) {
            WebrtcConnection::TurnServer turn;
            memset(&turn, 0, sizeof(turn));
            // TODO: do not use hardcoded credentials. We should use the credentials provided by the Basestation once our TURN servers are fixed.
            if (username != NULL) {
                turn.username = "pZn9wGCOD1MMgfJ2";//std::string(username);
            }
            if (cred != NULL) {
                turn.password = "0ZW1J5fvkJYbbpDA"; //std::string(cred);
            }
            turn.hostname = std::string(nabto_device_ice_servers_request_get_url(iceReq_, i, u));
            turnServers_.push_back(turn);
        }
    }
}

void SignalingStream::createWebrtcConnection() {
    auto self = shared_from_this();
    webrtcConnection_ = WebrtcConnection::create(self, device_, turnServers_, medias_, queue_);
    webrtcConnection_->setEventHandler([self](WebrtcConnection::ConnectionState state) {
        if (state == WebrtcConnection::ConnectionState::CLOSED ||
            state == WebrtcConnection::ConnectionState::FAILED) {
                self->closeStream();
        }
    });
}

void SignalingStream::sendSignalligObject(std::string& data)
{
    {
        std::lock_guard<std::mutex> lock(writeBuffersMutex_);
        writeBuffers_.push(data);
    }

    tryWriteStream();
}

void SignalingStream::tryWriteStream()
{
    std::string data;
    {
        std::lock_guard<std::mutex> lock(writeBuffersMutex_);
        if (writeBuf_ != NULL) {
            std::cout << "Write while writing" << std::endl;
            return;
        }

        if (writeBuffers_.empty()) {
           return;
        }
        data = writeBuffers_.front();
        writeBuffers_.pop();
    }

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
        // TODO: remove this mutex as it should be guarded by the event queue
        {
            std::lock_guard<std::mutex> lock(self->writeBuffersMutex_);
            free(self->writeBuf_);
            self->writeBuf_ = NULL;
        }
        self->tryWriteStream();
    });
}


void SignalingStream::readObjLength()
{
    if (closed_) {
        std::cout << "Read after closed. Self destruct" << std::endl;
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
    self->reading_ = false;
    // nabto_device_future_free(future);
    if (ec == NABTO_DEVICE_EC_EOF) {
        // make a nice shutdown
        printf("Read reached EOF closing nicely\n");
        self->queue_->post([self]() {
            self->closeStream();
        });
        return;
    }
    if (ec != NABTO_DEVICE_EC_OK) {
        std::cout << "Read failed with " << nabto_device_error_get_message(ec) << " cleaning up" << std::endl;
        self->queue_->post([self]() {
            self->cleanup();
        });
        return;
    }
    self->queue_->post([self]() {
        self->handleReadObjLen();
    });
}

void SignalingStream::handleReadObjLen()
{
    if (readLength_ < 4 || objectLength_ < 1) {
        std::cout << "Bad Length read: " << objectLength_ << std::endl;
        return readObjLength();
    }
    return readObject(objectLength_);
}

void SignalingStream::readObject(uint32_t len)
{
    if (closed_) {
        std::cout << "Read after closed. Self destruct" << std::endl;
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
        printf("Read reached EOF closing nicely\n");
        self->queue_->post([self]() {
            self->reading_ = false;
            self->closeStream();
        });
        return;
    }
    if (ec != NABTO_DEVICE_EC_OK) {
        std::cout << "Read failed with " << nabto_device_error_get_message(ec) << " cleaning up" << std::endl;
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
    if (readLength_ < objectLength_) {
        // we did not get all the data we wanted. This means the stream is closing down. We just read object length again to get the error code and clean up.
        return readObjLength();
    }

    nlohmann::json obj;
    try {
        obj = nlohmann::json::parse(objectBuffer_, objectBuffer_ + objectLength_);
        enum ObjectType type = static_cast<enum ObjectType>(obj["type"].get<int>());
        if (type == WEBRTC_OFFER) {
            auto offer = obj["data"].get<std::string>();
            nlohmann::json metadata = obj["metadata"];
            webrtcConnection_->setMetadata(metadata);
            webrtcConnection_->handleOffer(offer);
        }
        else if (type == WEBRTC_ANSWER) {
            auto answer = obj["data"].get<std::string>();
            webrtcConnection_->handleAnswer(answer);
        }
        else if (type == WEBRTC_ICE) {
            auto ice = obj["data"].get<std::string>();
            webrtcConnection_->handleIce(ice);
        }
        else if (type == TURN_REQUEST) {
            sendTurnServers();
        }
        else if (type == REQUEST_OFFER) {
            webrtcConnection_->handleOfferRequest();
        }
        else {
            std::cout << "Unknown object type: " << type << std::endl;
        }
    }
    catch (nlohmann::json::parse_error& ex) {
        std::cout << "Failed to parse JSON: " << ex.what() << std::endl;
        std::cout << "parsing: " << std::string((char*)objectBuffer_, objectLength_) << std::endl;
        free(objectBuffer_);
        objectBuffer_ = NULL;
        return readObjLength();
    }
    free(objectBuffer_);
    objectBuffer_ = NULL;
    return readObjLength();
}


void SignalingStream::signalingSendOffer(std::string& data, nlohmann::json& metadata)
{
    nlohmann::json msg = {
         {"type", WEBRTC_OFFER},
         {"data", data},
         {"metadata", metadata}
    };
    auto obj = msg.dump();
    sendSignalligObject(obj);
}

void SignalingStream::signalingSendAnswer(std::string& data, nlohmann::json& metadata)
{
    nlohmann::json msg = {
         {"type", WEBRTC_ANSWER},
         {"data", data},
         {"metadata", metadata}
    };
    auto obj = msg.dump();
    sendSignalligObject(obj);

}

void SignalingStream::signalingSendIce(std::string& data, nlohmann::json& metadata)
{
    nlohmann::json msg = {
         {"type", WEBRTC_ICE},
         {"data", data},
         {"metadata", metadata}
    };
    auto obj = msg.dump();
    sendSignalligObject(obj);
}


void SignalingStream::sendTurnServers()
{
    nlohmann::json resp = {
        {"type", ObjectType::TURN_RESPONSE},
        {"servers", nlohmann::json::array()}
    };
    for (auto t : turnServers_) {
        nlohmann::json turn = {
            {"hostname", t.hostname},
            {"port", t.port},
            {"username", t.username},
            {"password", t.password}
        };
        resp["servers"].push_back(turn);
    }
    auto str = resp.dump();
    sendSignalligObject(str);
}


void SignalingStream::closeStream()
{
    if (closed_) {
        if (!reading_ && writeBuf_ == NULL) {
            std::cout << "Got close when closed. Not reading not writing. cleaning up" << std::endl;
            cleanup();
        }
        return;
    }
    closed_ = true;
    NabtoDeviceFuture* closeFuture = nabto_device_future_new(device_->getDevice());
    nabto_device_stream_close(stream_, closeFuture);
    nabto_device_future_set_callback(closeFuture, streamClosed, this);
}

void SignalingStream::streamClosed(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    std::cout << "stream closed" << std::endl;
    (void)ec;
    nabto_device_future_free(future);
    SignalingStream* self = (SignalingStream*)userData;
    self->queue_->post([self]() {
        self->webrtcConnection_ = nullptr;
        if (!self->reading_ && self->writeBuf_ == NULL) {
            std::cout << "Not reading && writeBuf is NULL" << std::endl;
            self->cleanup();
        } else {
            std::cout << "reading or writing on closed. Awaiting self destuct" << std::endl;
        }
    });
}

void SignalingStream::cleanup()
{
    closed_ = true;
    if (webrtcConnection_ != nullptr) {
        webrtcConnection_->stop();
    }

    webrtcConnection_ = nullptr;
    self_ = nullptr;
}


} // namespace
