#pragma once

#include "webrtc_libdatachannel.hpp"

#include <nabto/nabto_device.h>
#include <nlohmann/json.hpp>

#include <memory>

using json = nlohmann::json;

namespace nabto {

#define READ_BUFFER_SIZE 1024


class WebrtcStream : public std::enable_shared_from_this<WebrtcStream> {
public:

    enum ObjectType {
        WEBRTC_OFFER = 0,
        WEBRTC_ANSWER,
        WEBRTC_ICE,
        TURN_REQUEST,
        TURN_RESPONSE
    };

    WebrtcStream(NabtoDevice* device, NabtoDeviceStream* stream) {
        device_ = device;
        stream_ = stream;
        fut_ = nabto_device_future_new(device);
        objectBuffer_ = NULL;

//        WebrtcChannel::TurnServer turn = { "3.252.194.140", 3478, "1675935678:foo", "D/9Nw9yGzXGL+qy/mvwLlXfOgVI=" };
        WebrtcChannel::TurnServer turn = { "34.245.62.208", 3478, "1683617751:foo", "ZdAQAp+mY+m0o+3WBVzxuS/g7Dk=" };

        turnServers_.push_back(turn);

        channel_ = std::make_shared<WebrtcChannel>(turnServers_);
    }

    ~WebrtcStream() {
        std::cout << "WebrtcStream Destructor" << std::endl;
        nabto_device_stream_free(stream_);
        nabto_device_future_free(fut_);
    }

    NabtoDeviceStream* stream() {
        return stream_;
    }

    void start() {
        auto self = shared_from_this();
        channel_->setSignalSender(
        [self](std::string& data) {
            self->sendSignalligObject(data);
        });
        channel_->setEventHandler(
            [self](enum WebrtcChannel::ConnectionEvent ev) {
                std::cout << "Got WebrtcChannel Connection Event: " << ev << std::endl;
                if (ev == WebrtcChannel::ConnectionEvent::CONNECTED) {
                    self->connected_ = true;
                }
                else if (ev == WebrtcChannel::ConnectionEvent::CLOSED) {
                    self->self_ = nullptr;
                }
        });
        nabto_device_stream_accept(stream_, fut_);
        self_ = shared_from_this();
        nabto_device_future_set_callback(fut_, streamAccepted, this);
    }

    void handleVideoData(uint8_t* buffer, size_t len)
    {
        if (connected_) {
            channel_->sendVideoData(buffer, len);
        }
    }

private:

    static void streamAccepted(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData) {
        WebrtcStream* self = (WebrtcStream*)userData;
        if (ec != NABTO_DEVICE_EC_OK) {
            self->self_ = nullptr;
            return;
        }
        self->readObjLength();
    }

    // READ OBJECT LENGTH //
    void readObjLength()
    {
        std::cout << "Reading Next Object length" << std::endl;
        objectLength_ = 0;
        NabtoDeviceFuture* readFuture = nabto_device_future_new(device_);
        nabto_device_stream_read_all(stream_, readFuture, &objectLength_, 4, &readLength_);
        nabto_device_future_set_callback(readFuture, hasReadObjLen, this);
    }

    static void hasReadObjLen(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
    {
        WebrtcStream* self = (WebrtcStream*)userData;
        nabto_device_future_free(future);
        std::cout << "Read length cb" << std::endl;
        if (ec == NABTO_DEVICE_EC_EOF) {
            // make a nice shutdown
            printf("Read reached EOF closing nicely\n");
            self->startClose();
            return;
        }
        if (ec != NABTO_DEVICE_EC_OK) {
            self->self_ = nullptr;
            return;
        }
        self->handleReadObjLen();
    }

    void handleReadObjLen()
    {
        if(readLength_ < 4 || objectLength_ < 1) {
            std::cout << "Bad Length read: " << objectLength_ << std::endl;
            return readObjLength();
        }
        return readObject(objectLength_);

    }

    // READ OBJECT //
    void readObject(uint32_t len)
    {
        std::cout << "Reading next object of length: " << len << std::endl;
        objectBuffer_ = (uint8_t*)calloc(1, len);
        NabtoDeviceFuture* readFuture = nabto_device_future_new(device_);
        nabto_device_stream_read_all(stream_, readFuture, objectBuffer_, len, &readLength_);
        nabto_device_future_set_callback(readFuture, hasReadObject, this);
    }

    static void hasReadObject(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
    {
        WebrtcStream* self = (WebrtcStream*)userData;
        nabto_device_future_free(future);
        if (ec == NABTO_DEVICE_EC_EOF) {
            // make a nice shutdown
            printf("Read reached EOF closing nicely\n");
            self->startClose();
            return;
        }
        if (ec != NABTO_DEVICE_EC_OK) {
            self->self_ = nullptr;
            return;
        }
        self->handleReadObject();
    }

    void handleReadObject()
    {
        if (readLength_ < objectLength_) {
            // we did not get all the data we wanted. This means the stream is closing down. We just read object length again to get the error code and clean up.
            return readObjLength();
        }

        json obj;
        try {
            obj = json::parse(objectBuffer_, objectBuffer_+objectLength_);
            enum ObjectType type = static_cast<enum ObjectType>(obj["type"].get<int>());
            if (type == WEBRTC_OFFER) {
                auto offer = obj["data"].get<std::string>();
                channel_->handleOffer(offer);
            } else if (type == WEBRTC_ANSWER) {
                auto answer = obj["data"].get<std::string>();
                channel_->handleAnswer(answer);
            } else if (type == WEBRTC_ICE) {
                auto ice = obj["data"].get<std::string>();
                channel_->handleIce(ice);
            } else if (type == TURN_REQUEST) {
                sendTurnServers();
                //channel_->handleTurnReq();
            } else {
                std::cout << "Unknown object type: " << type << std::endl;
            }
        } catch(json::parse_error& ex) {
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

    // TODO: should this be fire-and-forget by webrtc?
    // TODO: fix if write returns OPERATION IN PROGRESS
    void sendSignalligObject(std::string& data)
    {
        NabtoDeviceFuture* fut = nabto_device_future_new(device_);
        uint32_t size = data.size();
        void* buf = calloc(1, size+4);
        memcpy(buf, &size, 4);
        memcpy(buf+4, data.data(), size);
        nabto_device_stream_write(stream_, fut, buf, size+4);
        nabto_device_future_set_callback(fut, written, buf);
    }

    static void written(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
    {
        (void)ec;
        nabto_device_future_free(future);
        free(userData);
    }

    void sendTurnServers()
    {
        json resp = {
            {"type", ObjectType::TURN_RESPONSE},
            {"servers", json::array()}
        };
        for (auto t : turnServers_) {
            json turn = {
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

    void startClose()
    {
        NabtoDeviceFuture* closeFuture = nabto_device_future_new(device_);
        nabto_device_stream_close(stream_, closeFuture);
        nabto_device_future_set_callback(closeFuture, closed, this);
    }

    static void closed(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
    {
        (void)ec;
        nabto_device_future_free(future);
        WebrtcStream* self = (WebrtcStream*)userData;
        self->self_ = nullptr;
    }

    NabtoDevice* device_;
    NabtoDeviceStream* stream_;
    NabtoDeviceFuture* fut_;
    std::shared_ptr<WebrtcChannel> channel_;

    std::shared_ptr<WebrtcStream> self_;
    size_t readLength_;
    uint32_t objectLength_;
    uint8_t* objectBuffer_;
    bool connected_ = false;
    std::vector<WebrtcChannel::TurnServer> turnServers_;

};

typedef std::shared_ptr<WebrtcStream> WebrtcStreamPtr;
typedef std::weak_ptr<WebrtcStream> WebrtcStreamWeakPtr;

}
