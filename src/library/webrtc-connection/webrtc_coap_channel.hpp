#pragma once

#include <nabto/nabto_device_webrtc.hpp>
#include <nabto/nabto_device_virtual.h>

#include <rtc/rtc.hpp>

#include <nlohmann/json.hpp>

#include <memory>

namespace nabto {

class WebrtcCoapChannel;
typedef std::shared_ptr<WebrtcCoapChannel> WebrtcCoapChannelPtr;

class VirtualCoapRequest;

class WebrtcCoapChannel : public std::enable_shared_from_this < WebrtcCoapChannel>
{
public:

    enum coapMessageType {
        COAP_REQUEST = 0,
        COAP_RESPONSE
    };

    static WebrtcCoapChannelPtr create(std::shared_ptr<rtc::PeerConnection> pc, std::shared_ptr<rtc::DataChannel> channel, NabtoDevicePtr device, NabtoDeviceVirtualConnection* nabtoConnection, EventQueuePtr queue);

    WebrtcCoapChannel(std::shared_ptr<rtc::PeerConnection> pc, std::shared_ptr<rtc::DataChannel> channel, NabtoDevicePtr device, NabtoDeviceVirtualConnection* nabtoConnection, EventQueuePtr queue);

    ~WebrtcCoapChannel() {
        std::cout << "WebrtcCoapChannel Destructor" << std::endl;
    }

    void init();

    void handleStringMessage(const std::string& data);

private:

    void sendResponse(const nlohmann::json& response);

    std::shared_ptr<rtc::PeerConnection> pc_;
    std::shared_ptr<rtc::DataChannel> channel_;
    NabtoDevicePtr device_;

    NabtoDeviceVirtualConnection* nabtoConnection_;
    EventQueuePtr queue_;

};

} // Namespace
