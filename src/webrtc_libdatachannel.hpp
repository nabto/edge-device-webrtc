#pragma once

#include <nabto/nabto_device.h>
#include <nabto/nabto_device_experimental.h>
#include <nabto/nabto_device_virtual.h>
#include "rtc/rtc.hpp"

#include <iostream>

namespace nabto {

class WebrtcChannel: public std::enable_shared_from_this<WebrtcChannel> {
public:

    struct TurnServer {
        std::string hostname;
        uint16_t port;
        std::string username;
        std::string password;
    };

    enum ConnectionEvent {
        CONNECTED = 0,
        CLOSED,
        FAILED
    };

    enum controlMessageType {
        COAP_REQUEST = 0,
        COAP_RESPONSE
    };

    WebrtcChannel(std::vector<struct TurnServer> servers)
    {
        turnServers_ = servers;
    }

    ~WebrtcChannel()
    {
        if (nabtoConnection_ != NULL) {
            nabto_device_virtual_connection_free(nabtoConnection_);
        }
        std::cout << "WebrtcChannel Destructor" << std::endl;
    }

    void setNabtoDevice(NabtoDevice* device) {
        device_ = device;
    }

    void setEventHandler(std::function<void(enum ConnectionEvent)> eventHandler)
    {
        eventHandler_ = eventHandler;
    }

    void setSignalSender(std::function<void(std::string&)> sendSignal)
    {
        sendSignal_ = sendSignal;
    }

    void handleOffer(std::string& offer);

    void handleAnswer(std::string& answer);

    void handleIce(std::string& ice);

    void sendVideoData(uint8_t* buffer, size_t len)
    {
        if (len < sizeof(rtc::RtpHeader) || !track_ || !track_->isOpen())
            return;

        auto rtp = reinterpret_cast<rtc::RtpHeader*>(buffer);
        rtp->setSsrc(ssrc_);
        rtp->setPayloadType(dstPayloadType_);

        track_->send(reinterpret_cast<const std::byte*>(buffer), len);
    }

private:
    void createPeerConnection();

    static void coapCallback(NabtoDeviceFuture* fut, NabtoDeviceError err, void* data);

    void handleCoapResponse(NabtoDeviceError err);

    std::function<void(std::string&)> sendSignal_;
    std::function<void(enum ConnectionEvent)> eventHandler_;
    std::shared_ptr<rtc::PeerConnection> pc_ = nullptr;
    std::shared_ptr<rtc::DataChannel> coapChannel_;

    rtc::SSRC ssrc_ = 42;
    int srcPayloadType_ = 0;
    int dstPayloadType_ = 0;
    std::shared_ptr<rtc::Track> track_;
    rtc::PeerConnection::GatheringState state_ = rtc::PeerConnection::GatheringState::New;

    std::vector<struct TurnServer> turnServers_;
    NabtoDevice* device_ = NULL;
    NabtoDeviceVirtualConnection* nabtoConnection_ = NULL;
    NabtoDeviceVirtualCoapRequest* coap_ = NULL;
    std::string coapRequestId_;
};


}
