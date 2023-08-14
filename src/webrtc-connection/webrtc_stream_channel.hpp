#pragma once

#include <nabto-device/nabto_device.hpp>
#include <nabto/nabto_device_virtual.h>

#include <rtc/rtc.hpp>

#include <memory>

namespace nabto {

class WebrtcStreamChannel;
typedef std::shared_ptr<WebrtcStreamChannel> WebrtcStreamChannelPtr;

class VirtualCoapRequest;

class WebrtcStreamChannel : public std::enable_shared_from_this < WebrtcStreamChannel>
{
public:
    static WebrtcStreamChannelPtr create(std::shared_ptr<rtc::DataChannel> channel, NabtoDeviceImplPtr device, NabtoDeviceVirtualConnection* nabtoConnection);

    WebrtcStreamChannel(std::shared_ptr<rtc::DataChannel> channel, NabtoDeviceImplPtr device, NabtoDeviceVirtualConnection* nabtoConnection);

    ~WebrtcStreamChannel() {
        std::cout << "WebrtcStreamChannel Destructor" << std::endl;
    }

    void init();

private:

    static void streamOpened(NabtoDeviceFuture* fut, NabtoDeviceError ec, void* data);

    void startRead();
    static void streamReadCb(NabtoDeviceFuture* fut, NabtoDeviceError ec, void* data);

    std::shared_ptr<rtc::DataChannel> channel_;
    NabtoDeviceImplPtr device_;

    NabtoDeviceVirtualConnection* nabtoConnection_;

    NabtoDeviceVirtualStream* nabtoStream_;
    NabtoDeviceFuture* future_;
    uint8_t readBuffer_[1024];
    size_t readLen_ = 0;

};

} // Namespace
