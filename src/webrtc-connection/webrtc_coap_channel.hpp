#pragma once

#include <nabto-device/nabto_device.hpp>
#include <nabto/nabto_device_virtual.h>

#include <rtc/rtc.hpp>

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

    static WebrtcCoapChannelPtr create(std::shared_ptr<rtc::PeerConnection> pc, std::shared_ptr<rtc::DataChannel> channel, NabtoDeviceImplPtr device, NabtoDeviceVirtualConnection* nabtoConnection, EventQueuePtr queue);

    WebrtcCoapChannel(std::shared_ptr<rtc::PeerConnection> pc, std::shared_ptr<rtc::DataChannel> channel, NabtoDeviceImplPtr device, NabtoDeviceVirtualConnection* nabtoConnection, EventQueuePtr queue);

    ~WebrtcCoapChannel() {
        std::cout << "WebrtcCoapChannel Destructor" << std::endl;
    }

    void init();

    void handleStringMessage(std::string& data);

private:

    void sendResponse(const nlohmann::json& response);

    std::shared_ptr<rtc::PeerConnection> pc_;
    std::shared_ptr<rtc::DataChannel> channel_;
    NabtoDeviceImplPtr device_;

    NabtoDeviceVirtualConnection* nabtoConnection_;
    EventQueuePtr queue_;

};

} // Namespace
