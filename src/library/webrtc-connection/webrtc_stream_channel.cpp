#include "webrtc_stream_channel.hpp"

#include <nlohmann/json.hpp>

namespace nabto {

WebrtcStreamChannelPtr WebrtcFileStreamChannel::create(std::shared_ptr<rtc::DataChannel> channel, NabtoDevicePtr device, NabtoDeviceVirtualConnection* nabtoConnection, uint32_t streamPort, EventQueuePtr queue)
{
    auto ptr = std::make_shared<WebrtcFileStreamChannel>(channel, device, nabtoConnection, streamPort, queue);
    ptr->init();
    return ptr;
}

WebrtcFileStreamChannel::WebrtcFileStreamChannel(std::shared_ptr<rtc::DataChannel> channel, NabtoDevicePtr device, NabtoDeviceVirtualConnection* nabtoConnection, uint32_t streamPort, EventQueuePtr queue)
    : channel_(channel), device_(device), nabtoConnection_(nabtoConnection), streamPort_(streamPort), queue_(queue)
{
}

void WebrtcFileStreamChannel::init()
{
    auto self = shared_from_this();
    channel_->onMessage([self](rtc::binary data) {
        NPLOGD << "Got stream Data channel binary data"
           ;
        },
        [self](std::string data) {
            NPLOGD
                << "Got stream data channel string data: " << data
               ;
    });

    channel_->onClosed([self]() {
        NPLOGD << "Stream Channel Closed";
        self->channel_ = nullptr;
    });

    NPLOGD << "Stream channel created, opening nabto stream";

    future_ = nabto_device_future_new(device_.get());
    nabtoStream_= nabto_device_virtual_stream_new(nabtoConnection_);
    nabto_device_virtual_stream_open(nabtoStream_, future_, streamPort_);
    nabto_device_future_set_callback(future_, streamOpened, this);
}

void WebrtcFileStreamChannel::streamOpened(NabtoDeviceFuture* fut, NabtoDeviceError ec, void* data)
{
    WebrtcFileStreamChannel* self = (WebrtcFileStreamChannel*)data;
    if (ec != NABTO_DEVICE_EC_OK) {
        NPLOGE << "Stream open failed";
        // TODO: handle error
        return;
    }
    NPLOGD << "Nabto stream opened";
    self->queue_->post([self]() {
        self->startRead();
    });
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
        NPLOGD << "Reached EOF";
        self->queue_->post([self]() {
            self->channel_->close();
            self->channel_ = nullptr;
        });
        return;
    } else if (ec != NABTO_DEVICE_EC_OK) {
        NPLOGE << "Stream read failed with: " << nabto_device_error_get_message(ec);
        // TODO: handle error
        return;
    }
    NPLOGD << "Read " << self->readLen_ << "bytes from nabto stream";
    // TODO: consider handling return value and react to buffered messages
    self->queue_->post([self]() {
        self->channel_->send((rtc::byte*)self->readBuffer_, self->readLen_);
        self->startRead();
    });
}

} // namespace
