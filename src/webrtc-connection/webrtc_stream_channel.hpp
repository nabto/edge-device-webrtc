#pragma once

#include <nabto-device/nabto_device.hpp>
#include <nabto/nabto_device_virtual.h>

#include <rtc/rtc.hpp>

#include <memory>

namespace nabto {

class WebrtcFileStreamChannel;
typedef std::shared_ptr<WebrtcFileStreamChannel> WebrtcStreamChannelPtr;

class VirtualCoapRequest;

class WebrtcFileStreamChannel : public std::enable_shared_from_this < WebrtcFileStreamChannel>
{
public:
    static WebrtcStreamChannelPtr create(std::shared_ptr<rtc::DataChannel> channel, NabtoDeviceImplPtr device, NabtoDeviceVirtualConnection* nabtoConnection, uint32_t streamPort, EventQueuePtr queue);

    WebrtcFileStreamChannel(std::shared_ptr<rtc::DataChannel> channel, NabtoDeviceImplPtr device, NabtoDeviceVirtualConnection* nabtoConnection, uint32_t streamPort, EventQueuePtr queue);

    ~WebrtcFileStreamChannel() {
        std::cout << "WebrtcFileStreamChannel Destructor" << std::endl;
    }

    void init();

private:

    static void streamOpened(NabtoDeviceFuture* fut, NabtoDeviceError ec, void* data);

    void startRead();
    static void streamReadCb(NabtoDeviceFuture* fut, NabtoDeviceError ec, void* data);

    std::shared_ptr<rtc::DataChannel> channel_;
    NabtoDeviceImplPtr device_;
    NabtoDeviceVirtualConnection* nabtoConnection_;
    uint32_t streamPort_ = 0;
    EventQueuePtr queue_;

    NabtoDeviceVirtualStream* nabtoStream_;
    NabtoDeviceFuture* future_;
    uint8_t readBuffer_[1024];
    size_t readLen_ = 0;

};

} // Namespace
