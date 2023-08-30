#include "signaling_stream_manager.hpp"

#include <nlohmann/json.hpp>

namespace nabto {

const char* coapInfoPath[] = { "webrtc", "info", NULL };
const char* coapVideoPath[] = { "webrtc", "video", "{feed}", NULL };

SignalingStreamManagerPtr SignalingStreamManager::create(NabtoDeviceImplPtr device, std::vector<nabto::MediaStreamPtr>& medias)
{
    return std::make_shared<SignalingStreamManager>(device, medias);
}

SignalingStreamManager::SignalingStreamManager(NabtoDeviceImplPtr device, std::vector<nabto::MediaStreamPtr>& medias) : device_(device), medias_(medias)
{
    streamListener_ = nabto_device_listener_new(device_->getDevice());
    streamListenFuture_ = nabto_device_future_new(device_->getDevice());
    coapInfoListener_ = nabto_device_listener_new(device_->getDevice());
    coapInfoListenFuture_ = nabto_device_future_new(device_->getDevice());
    coapVideoListener_ = nabto_device_listener_new(device_->getDevice());
    coapVideoListenFuture_ = nabto_device_future_new(device_->getDevice());
}

SignalingStreamManager::~SignalingStreamManager()
{
    std::cout << "SignalingStreamManager Destructor" << std::endl;
    nabto_device_future_free(coapInfoListenFuture_);
    nabto_device_listener_free(coapInfoListener_);
    nabto_device_future_free(coapVideoListenFuture_);
    nabto_device_listener_free(coapVideoListener_);
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

    ec = nabto_device_coap_init_listener(device_->getDevice(), coapInfoListener_, NABTO_DEVICE_COAP_GET, coapInfoPath);
    if (ec != NABTO_DEVICE_EC_OK) {
        std::cout << "Failed to initialize coap info listener with error: " << nabto_device_error_get_message(ec) << std::endl;
        return false;
    }

    ec = nabto_device_coap_init_listener(device_->getDevice(), coapVideoListener_, NABTO_DEVICE_COAP_GET, coapVideoPath);
    if (ec != NABTO_DEVICE_EC_OK) {
        std::cout << "Failed to initialize coap video listener with error: " << nabto_device_error_get_message(ec) << std::endl;
        return false;
    }

    // Keep myself alive until listeners are closed
    me_ = shared_from_this();
    nextStream();
    nextInfoRequest();
    nextVideoRequest();
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

void SignalingStreamManager::nextInfoRequest()
{
    nabto_device_listener_new_coap_request(coapInfoListener_, coapInfoListenFuture_, &coapInfoRequest_);
    nabto_device_future_set_callback(coapInfoListenFuture_, newInfoRequest, this);
}

void SignalingStreamManager::newInfoRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    SignalingStreamManager* self = (SignalingStreamManager*)userData;
    if (ec != NABTO_DEVICE_EC_OK)
    {
        std::cout << "CoAP future wait failed: " << nabto_device_error_get_message(ec) << std::endl;
        // TODO: only reset me_ if stream and video is also closed
        self->me_ = nullptr;
        self->device_ = nullptr;
        return;
    }
    // TODO: check IAM
    if (true) //self->accessCb_(nabto_device_stream_get_connection_ref(self->stream_), streamAction, self->accessUserData_))
    {
        nlohmann::json resp = {
            {"SignalingStreamPort", self->streamPort_},
            {"FileStreamPort", self->device_->getFileStreamPort()}
        };
        std::cout << "Sending info response: " << resp.dump() << std::endl;
        auto payload = nlohmann::json::to_cbor(resp);
        nabto_device_coap_response_set_code(self->coapInfoRequest_, 205);
        nabto_device_coap_response_set_content_format(self->coapInfoRequest_, NABTO_DEVICE_COAP_CONTENT_FORMAT_APPLICATION_CBOR);
        nabto_device_coap_response_set_payload(self->coapInfoRequest_, payload.data(), payload.size());
        nabto_device_coap_response_ready(self->coapInfoRequest_);
    }
    else {
        nabto_device_coap_error_response(self->coapInfoRequest_, 401, NULL);
    }
    nabto_device_coap_request_free(self->coapInfoRequest_);
    self->nextInfoRequest();

}

void SignalingStreamManager::nextVideoRequest()
{
    std::cout << "Getting next coap request" << std::endl;
    nabto_device_listener_new_coap_request(coapVideoListener_, coapVideoListenFuture_, &coapVideoRequest_);
    nabto_device_future_set_callback(coapVideoListenFuture_, newVideoRequest, this);
}

void SignalingStreamManager::newVideoRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    SignalingStreamManager* self = (SignalingStreamManager*)userData;
    if (ec != NABTO_DEVICE_EC_OK)
    {
        std::cout << "CoAP future wait failed: " << nabto_device_error_get_message(ec) << std::endl;
        // TODO: only reset me_ if stream and info is also closed
        self->me_ = nullptr;
        self->device_ = nullptr;
        return;
    }
    std::cout << "Got new coap request" << std::endl;
    self->handleVideoRequest();
    std::cout << "Freeing coap request" << std::endl;
    nabto_device_coap_request_free(self->coapVideoRequest_);
    self->nextVideoRequest();
}

void SignalingStreamManager::handleVideoRequest()
{
    // TODO: check IAM
    if (true) {
        std::cout << "Handling video coap request" << std::endl;
        const char* feedC = nabto_device_coap_request_get_parameter(coapVideoRequest_, "feed");
        std::string feed(feedC);
        NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coapVideoRequest_);

        SignalingStreamPtr stream = nullptr;

        for (auto p : streams_) {
            auto ptr = p.lock();
            if (ptr && ptr->isConnection(ref)) {
                stream = ptr;
                break;
            }
        }

        if (stream == nullptr) {
            std::cout << "Did not find matching singaling stream" << std::endl;
            nabto_device_coap_error_response(coapVideoRequest_, 400, "Invalid connection");
            return;
        }

        bool found = false;

        // TODO: feed should represent eg. frontdoor, so we should look for both frontdoor-video and frontdoor-audio. Change trackId to be "frontdoor" and add type field to the media.
        for (auto m : medias_) {
            auto id = m->getTrackId();
            if (id == feed || id == (feed + "-video") || id == (feed + "-audio")) {
                found = true;
                if(!stream->createTrack(m)) {
                    std::cout << "Failed to create track for video feed" << std::endl;
                    nabto_device_coap_error_response(coapVideoRequest_, 400, "Invalid connection");
                    return;
                }
            }
        }

        if (found) {
            std::cout << "found track ID in feeds returning 201" << std::endl;
            nabto_device_coap_response_set_code(coapVideoRequest_, 201);
            nabto_device_coap_response_ready(coapVideoRequest_);
        } else {
            std::cout << "Failed to find track for video feed" << std::endl;
            nabto_device_coap_error_response(coapVideoRequest_, 404, "No such feed");
        }
    } else {
        nabto_device_coap_error_response(coapVideoRequest_, 401, NULL);
    }
}


} // namespace
