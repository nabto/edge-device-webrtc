#include "signaling_stream_manager.hpp"

#include <nlohmann/json.hpp>

namespace nabto {

const char* coapPath[] = {"webrtc", "info", NULL};

SignalingStreamManagerPtr SignalingStreamManager::create(NabtoDeviceImplPtr device, std::vector<nabto::MediaStreamPtr>& medias)
{
    return std::make_shared<SignalingStreamManager>(device, medias);
}

SignalingStreamManager::SignalingStreamManager(NabtoDeviceImplPtr device, std::vector<nabto::MediaStreamPtr>& medias) : device_(device), medias_(medias)
{
    streamListener_ = nabto_device_listener_new(device_->getDevice());
    streamListenFuture_ = nabto_device_future_new(device_->getDevice());
    coapListener_ = nabto_device_listener_new(device_->getDevice());
    coapListenFuture_ = nabto_device_future_new(device_->getDevice());
}

SignalingStreamManager::~SignalingStreamManager()
{
    std::cout << "SignalingStreamManager Destructor" << std::endl;
    nabto_device_future_free(coapListenFuture_);
    nabto_device_listener_free(coapListener_);
    nabto_device_future_free(streamListenFuture_);
    nabto_device_listener_free(streamListener_);
}

bool SignalingStreamManager::start()
{
    if (streamListener_ == NULL || streamListenFuture_ == NULL) {
        std::cout << "Failed to create stream listener or its future" << std::endl;
        return false;
    }

    NabtoDeviceError ec;
    ec = nabto_device_stream_init_listener_ephemeral(device_->getDevice(), streamListener_, &streamPort_);
    if (ec != NABTO_DEVICE_EC_OK) {
        std::cout << "Failed to initialize stream listener with error: " << nabto_device_error_get_message(ec) << std::endl;
        return false;
    }

    ec = nabto_device_coap_init_listener(device_->getDevice(), coapListener_, NABTO_DEVICE_COAP_GET, coapPath);
    if (ec != NABTO_DEVICE_EC_OK) {
        std::cout << "Failed to initialize coap listener with error: " << nabto_device_error_get_message(ec) << std::endl;
        return false;
    }

    // Keep myself alive until listeners are closed
    me_ = shared_from_this();
    nextStream();
    nextCoapRequest();
    return true;
}

void SignalingStreamManager::nextStream()
{
    nabto_device_listener_new_stream(streamListener_, streamListenFuture_, &stream_);
    nabto_device_future_set_callback(streamListenFuture_, newStream, this);
}

void SignalingStreamManager::newStream(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    SignalingStreamManager* self = (SignalingStreamManager*)userData;
    if (ec != NABTO_DEVICE_EC_OK)
    {
        std::cout << "stream future wait failed: " << nabto_device_error_get_message(ec) << std::endl;
        // TODO: only reset me_ if coap is also closed
        self->me_ = nullptr;
        self->device_ = nullptr;
        return;
    }
    // TODO: check IAM
    if (true) //self->accessCb_(nabto_device_stream_get_connection_ref(self->stream_), streamAction, self->accessUserData_))
    {
        std::cout << "Creating Signaling stream" << std::endl;
        SignalingStreamPtr s = SignalingStream::create(self->device_, self->stream_, self->me_, self->medias_);
        self->streams_.push_back(s);
        s->start();
    }
    else {
        nabto_device_stream_free(self->stream_);
    }
    self->stream_ = NULL;
    self->nextStream();

}

void SignalingStreamManager::nextCoapRequest()
{
    nabto_device_listener_new_coap_request(coapListener_, coapListenFuture_, &coapRequest_);
    nabto_device_future_set_callback(coapListenFuture_, newCoapRequest, this);
}

void SignalingStreamManager::newCoapRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    SignalingStreamManager* self = (SignalingStreamManager*)userData;
    if (ec != NABTO_DEVICE_EC_OK)
    {
        std::cout << "CoAP future wait failed: " << nabto_device_error_get_message(ec) << std::endl;
        // TODO: only reset me_ if stream is also closed
        self->me_ = nullptr;
        self->device_ = nullptr;
        return;
    }
    // TODO: check IAM
    if (true) //self->accessCb_(nabto_device_stream_get_connection_ref(self->stream_), streamAction, self->accessUserData_))
    {
        nlohmann::json resp = {
            {"SignalingStreamPort", self->streamPort_}
        };
        std::cout << "Sending info response: " << resp.dump() << std::endl;
        auto payload = nlohmann::json::to_cbor(resp);
        nabto_device_coap_response_set_code(self->coapRequest_, 205);
        nabto_device_coap_response_set_content_format(self->coapRequest_, NABTO_DEVICE_COAP_CONTENT_FORMAT_APPLICATION_CBOR);
        nabto_device_coap_response_set_payload(self->coapRequest_, payload.data(), payload.size());
        nabto_device_coap_response_ready(self->coapRequest_);
    } else {
        nabto_device_coap_error_response(self->coapRequest_, 401, NULL);
    }
    nabto_device_coap_request_free(self->coapRequest_);
    self->nextCoapRequest();

}


} // namespace
