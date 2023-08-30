#include "webrtc_stream_channel.hpp"

#include <nlohmann/json.hpp>

namespace nabto {

WebrtcStreamChannelPtr WebrtcFileStreamChannel::create(std::shared_ptr<rtc::DataChannel> channel, NabtoDeviceImplPtr device, NabtoDeviceVirtualConnection* nabtoConnection, uint32_t streamPort)
{
    auto ptr = std::make_shared<WebrtcFileStreamChannel>(channel, device, nabtoConnection, streamPort);
    ptr->init();
    return ptr;
}

WebrtcFileStreamChannel::WebrtcFileStreamChannel(std::shared_ptr<rtc::DataChannel> channel, NabtoDeviceImplPtr device, NabtoDeviceVirtualConnection* nabtoConnection, uint32_t streamPort)
    : channel_(channel), device_(device), nabtoConnection_(nabtoConnection), streamPort_(streamPort)
{
}

void WebrtcFileStreamChannel::init()
{
    auto self = shared_from_this();
    channel_->onMessage([self](rtc::binary data) {
        std::cout << "Got stream Data channel binary data"
            << std::endl;
        },
        [self](std::string data) {
            std::cout
                << "Got stream data channel string data: " << data
                << std::endl;
    });

    channel_->onClosed([self]() {
        std::cout << "Stream Channel Closed" << std::endl;
        self->channel_ = nullptr;
    });

    std::cout << "Stream channel created, opening nabto stream" << std::endl;

    future_ = nabto_device_future_new(device_->getDevice());
    nabtoStream_= nabto_device_virtual_stream_new(nabtoConnection_);
    // TODO: ephemeral stream port
    nabto_device_virtual_stream_open(nabtoStream_, future_, streamPort_);
    nabto_device_future_set_callback(future_, streamOpened, this);
}

void WebrtcFileStreamChannel::streamOpened(NabtoDeviceFuture* fut, NabtoDeviceError ec, void* data)
{
    WebrtcFileStreamChannel* self = (WebrtcFileStreamChannel*)data;
    if (ec != NABTO_DEVICE_EC_OK) {
        std::cout << "Stream open failed" << std::endl;
        // TODO: handle error
        return;
    }
    std::cout << "Nabto stream opened" << std::endl;
    self->startRead();
}

void WebrtcFileStreamChannel::startRead()
{
    nabto_device_virtual_stream_read_some(nabtoStream_, future_, readBuffer_, 1024, &readLen_);
    nabto_device_future_set_callback(future_, streamReadCb, this);
}

void WebrtcFileStreamChannel::streamReadCb(NabtoDeviceFuture* fut, NabtoDeviceError ec, void* data)
{
    WebrtcFileStreamChannel* self = (WebrtcFileStreamChannel*)data;
    if (ec == NABTO_DEVICE_EC_EOF) {
        std::cout << "Reached EOF" << std::endl;
        self->channel_->close();
        self->channel_ = nullptr;
        return;
    } else if (ec != NABTO_DEVICE_EC_OK) {
        std::cout << "Stream read failed with: " << nabto_device_error_get_message(ec) << std::endl;
        // TODO: handle error
        return;
    }
    std::cout << "Read " << self->readLen_ << "bytes from nabto stream" << std::endl;
    // TODO: consider handling return value and react to buffered messages
    self->channel_->send((rtc::byte*)self->readBuffer_, self->readLen_);
    self->startRead();
}

} // namespace
