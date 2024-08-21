#include "coap_listener.hpp"

#include <nabto/nabto_device_experimental.h>
#include <nabto/nabto_device_virtual.h>


CoapListenerPtr CoapListener::create(nabto::NabtoDevicePtr device, NabtoDeviceCoapMethod method, const char** path, nabto::EventQueuePtr queue)
{
    auto ptr = std::make_shared<CoapListener>(device, queue);
    if (ptr->start(method, path)) {
        return ptr;
    }
    return nullptr;

}

CoapListener::CoapListener(nabto::NabtoDevicePtr device, nabto::EventQueuePtr queue) : device_(device), queue_(queue)
{
    listener_ = nabto_device_listener_new(device_.get());
    future_ = nabto_device_future_new(device_.get());

}

CoapListener::~CoapListener()
{
    nabto_device_future_free(future_);
    nabto_device_listener_free(listener_);

}

bool CoapListener::start(NabtoDeviceCoapMethod method, const char** path)
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

void CoapListener::nextCoapRequest()
{
    nabto_device_listener_new_coap_request(listener_, future_, &coap_);
    nabto_device_future_set_callback(future_, newCoapRequest, this);

}

void CoapListener::newCoapRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    CoapListener* self = (CoapListener*)userData;
    if (ec != NABTO_DEVICE_EC_OK)
    {
        std::cout << "Coap listener future wait failed: " << nabto_device_error_get_message(ec) << std::endl;
        self->queue_->post([self]() {
            self->me_ = nullptr;
            self->coapCb_ = nullptr;
            self->device_ = nullptr;
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
