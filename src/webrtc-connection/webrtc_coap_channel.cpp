#include "webrtc_coap_channel.hpp"

namespace nabto {

WebrtcCoapChannelPtr WebrtcCoapChannel::create(std::shared_ptr<rtc::DataChannel> channel, NabtoDeviceImplPtr device, NabtoDeviceVirtualConnection* nabtoConnection)
{
    return std::make_shared<WebrtcCoapChannel>(channel, device, nabtoConnection);
}

WebrtcCoapChannel::WebrtcCoapChannel(std::shared_ptr<rtc::DataChannel> channel, NabtoDeviceImplPtr device, NabtoDeviceVirtualConnection* nabtoConnection)
    : channel_(channel), device_(device), nabtoConnection_(nabtoConnection)
{

}


} // Namespace
