#include "nabto_listeners.hpp"

#include <nabto/nabto_device_experimental.h>
#include <nabto/nabto_device_virtual.h>
#include <modules/iam/nm_iam_serializer.h>

namespace nabto {

NabtoStreamListenerPtr NabtoStreamListener::create(NabtoDevicePtr device, EventQueuePtr queue)
{
    auto ptr = std::make_shared<NabtoStreamListener>(device, queue);
    if (ptr->start()) {
        return ptr;
    }
    return nullptr;
}

NabtoStreamListener::NabtoStreamListener(NabtoDevicePtr device, EventQueuePtr queue) : device_(device), queue_(queue)
{
    streamListen_ = nabto_device_listener_new(device_.get());
    streamFut_ = nabto_device_future_new(device_.get());
}

NabtoStreamListener::~NabtoStreamListener()
{
    nabto_device_future_free(streamFut_);
    nabto_device_listener_free(streamListen_);
}

bool NabtoStreamListener::start()
{
    if (streamListen_ == NULL ||
        streamFut_ == NULL ||
        nabto_device_stream_init_listener_ephemeral(device_.get(), streamListen_, &streamPort_) != NABTO_DEVICE_EC_OK)
    {
        std::cout << "Failed to listen for streams" << std::endl;
        return false;
    }
    me_ = shared_from_this();
    nextStream();
    return true;

}

void NabtoStreamListener::nextStream()
{
    nabto_device_listener_new_stream(streamListen_, streamFut_, &stream_);
    nabto_device_future_set_callback(streamFut_, newStream, this);
}

void NabtoStreamListener::newStream(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    NabtoStreamListener* self = (NabtoStreamListener*)userData;
    if (ec != NABTO_DEVICE_EC_OK)
    {
        std::cout << "stream future wait failed: " << nabto_device_error_get_message(ec) << std::endl;
        self->queue_->post([self]() {
            self->me_ = nullptr;
            self->streamCb_ = nullptr;
            self->device_ = nullptr;
            });
        return;
    }
    std::function<void(NabtoDeviceStream* coap)> cb = self->streamCb_;
    NabtoDeviceStream* stream = self->stream_;
    self->queue_->post([cb, stream]() {
        cb(stream);
        });
    self->stream_ = NULL;
    self->nextStream();

}


NabtoCoapListenerPtr NabtoCoapListener::create(NabtoDevicePtr device, NabtoDeviceCoapMethod method, const char** path, EventQueuePtr queue)
{
    auto ptr = std::make_shared<NabtoCoapListener>(device, queue);
    if (ptr->start(method, path)) {
        return ptr;
    }
    return nullptr;

}

NabtoCoapListener::NabtoCoapListener(NabtoDevicePtr device, EventQueuePtr queue) : device_(device), queue_(queue)
{
    listener_ = nabto_device_listener_new(device_.get());
    future_ = nabto_device_future_new(device_.get());

}

NabtoCoapListener::~NabtoCoapListener()
{
    nabto_device_future_free(future_);
    nabto_device_listener_free(listener_);

}

bool NabtoCoapListener::start(NabtoDeviceCoapMethod method, const char** path)
{
    if (listener_ == NULL ||
        future_ == NULL ||
        nabto_device_coap_init_listener(device_.get(), listener_, method, path) != NABTO_DEVICE_EC_OK)
    {
        std::cout << "Failed to listen for CoAP requests" << std::endl;
        return false;
    }
    me_ = shared_from_this();
    nextCoapRequest();
    return true;

}

void NabtoCoapListener::nextCoapRequest()
{
    nabto_device_listener_new_coap_request(listener_, future_, &coap_);
    nabto_device_future_set_callback(future_, newCoapRequest, this);

}

void NabtoCoapListener::newCoapRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    NabtoCoapListener* self = (NabtoCoapListener*)userData;
    if (ec != NABTO_DEVICE_EC_OK)
    {
        std::cout << "Coap listener future wait failed: " << nabto_device_error_get_message(ec) << std::endl;
        self->queue_->post([self]() {
            self->device_ = nullptr;
            self->coapCb_ = nullptr;
            self->me_ = nullptr;
        });
        return;
    }
    std::function<void(NabtoDeviceCoapRequest* coap)> cb = self->coapCb_;
    NabtoDeviceCoapRequest* req = self->coap_;
    self->queue_->post([cb, req]() {
        cb(req);
    });
    self->coap_ = NULL;
    self->nextCoapRequest();
}



} // Namespace
