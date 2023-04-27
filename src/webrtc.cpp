#include "webrtc.hpp"
#include <iostream>

namespace nabto {

const char* streamAction = "Webrtc:Stream";

NabtoWebrtc::NabtoWebrtc(NabtoDevice* dev, check_access checkAccess, void* userData)
{
    std::cout << "HELLO WORLD" << std::endl;
    device_ = dev;
    accessCb_ = checkAccess;
    accessUserData_ = userData;
    streamListener_ = nabto_device_listener_new(dev);
    nabto_device_stream_init_listener(dev, streamListener_, 42);
    streamListenFuture_ = nabto_device_future_new(dev);
}

NabtoWebrtc::~NabtoWebrtc()
{
    nabto_device_listener_free(streamListener_);
    nabto_device_future_free(streamListenFuture_);
}

void NabtoWebrtc::run()
{
    NabtoDeviceStream* stream;
    NabtoDeviceError ec;
    // WebrtcStream stream;
    while (true)
    {
        // TODO: This should also handle read from UDP socket
        nabto_device_listener_new_stream(streamListener_, streamListenFuture_, &stream);
        ec = nabto_device_future_wait(streamListenFuture_);
        if (ec != NABTO_DEVICE_EC_OK)
        {
            std::cout << "stream future wait failed: " << nabto_device_error_get_message(ec) << std::endl;
            break;
        }
        if (accessCb_(nabto_device_stream_get_connection_ref(stream), streamAction, accessUserData_))
        {
            WebrtcStreamPtr s = std::make_shared<WebrtcStream>(device_, stream);
            streams_.push_back(s);
            s->start();
            stream = NULL;
        } else {
            nabto_device_stream_free(stream);
        }
    }
}

} // namespace
