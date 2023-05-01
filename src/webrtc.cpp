#include "webrtc.hpp"
#include <iostream>

namespace nabto {

const char* streamAction = "Webrtc:Stream";

NabtoWebrtc::NabtoWebrtc(NabtoDevice* dev, check_access checkAccess, void* userData)
{
    device_ = dev;
    accessCb_ = checkAccess;
    accessUserData_ = userData;
    streamListener_ = nabto_device_listener_new(dev);
    nabto_device_stream_init_listener(dev, streamListener_, 42);
    streamListenFuture_ = nabto_device_future_new(dev);
}

NabtoWebrtc::~NabtoWebrtc()
{
    std::cout << "NabtoWebrtc Destructor" << std::endl;
    nabto_device_listener_free(streamListener_);
    nabto_device_future_free(streamListenFuture_);
}

void NabtoWebrtc::start()
{
    NabtoDeviceError ec;
    nabto_device_listener_new_stream(streamListener_, streamListenFuture_, &stream_);
    nabto_device_future_set_callback(streamListenFuture_, newStream, this);
}

void NabtoWebrtc::newStream(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    (void)ec;
    NabtoWebrtc* self = (NabtoWebrtc*)userData;
    if (ec != NABTO_DEVICE_EC_OK)
    {
        std::cout << "stream future wait failed: " << nabto_device_error_get_message(ec) << std::endl;
        return;
    }
    if (self->accessCb_(nabto_device_stream_get_connection_ref(self->stream_), streamAction, self->accessUserData_))
    {
        WebrtcStreamPtr s = std::make_shared<WebrtcStream>(self->device_, self->stream_);
        self->streams_.push_back(s);
        s->start();
    }
    else {
        nabto_device_stream_free(self->stream_);
    }
    self->stream_ = NULL;
    self->start();
}

void NabtoWebrtc::handleVideoData(uint8_t* buffer, size_t len)
{
    for (auto s = streams_.begin(); s < streams_.end(); s++) {
        auto stream = s->lock();
        if (stream) {
            stream->handleVideoData(buffer, len);
        } else {
            streams_.erase(s);
        }
    }
}

} // namespace
