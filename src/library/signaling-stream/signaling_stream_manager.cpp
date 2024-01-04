#include "signaling_stream_manager.hpp"

#include <nlohmann/json.hpp>

namespace nabto {

const char* coapInfoPath[] = { "webrtc", "info", NULL };

SignalingStreamManagerPtr SignalingStreamManager::create(NabtoDevicePtr device, EventQueuePtr queue)
{
    return std::make_shared<SignalingStreamManager>(device, queue);
}

SignalingStreamManager::SignalingStreamManager(NabtoDevicePtr device, EventQueuePtr queue) : device_(device), queue_(queue)
{
    streamListener_ = NabtoStreamListener::create(device_, queue_);
    coapInfoListener_ = NabtoCoapListener::create(device_, NABTO_DEVICE_COAP_GET, coapInfoPath, queue_);
}

SignalingStreamManager::~SignalingStreamManager()
{
    std::cout << "SignalingStreamManager Destructor" << std::endl;
}

bool SignalingStreamManager::start()
{
    auto self = shared_from_this();
    streamListener_->setStreamCallback([self](NabtoDeviceStream* stream) {
        NabtoDeviceConnectionRef ref = nabto_device_stream_get_connection_ref(stream);
        if (self->accessCb_ && self->accessCb_(ref, "Webrtc:Signaling"))
        {
            char* fp;
            nabto_device_connection_get_client_fingerprint(self->device_.get(), ref, &fp);
            std::cout << "Creating Signaling stream for client fp: " << fp << std::endl;
            nabto_device_string_free(fp);
            SignalingStreamPtr s = SignalingStream::create(self->device_, stream, self, self->queue_,
                [self](NabtoDeviceConnectionRef connRef, MediaTrackPtr track) {
                    self->trackCb_(connRef, track);
                },
                [self](NabtoDeviceConnectionRef ref, std::string action) -> bool {
                    return self->accessCb_(ref, action);
                });
            self->streams_.push_back(s);
            s->start();
        }
        else {
            std::cout << "New signaling stream opened, but IAM rejected it" << std::endl;
            nabto_device_stream_free(stream);
        }
    });

    coapInfoListener_->setCoapCallback([self](NabtoDeviceCoapRequest* coap) {
        NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

        if (self->accessCb_ && self->accessCb_(ref, "Webrtc:GetInfo"))
        {
            nlohmann::json resp = {
                {"SignalingStreamPort", self->streamListener_->getStreamPort()}
            };
            std::cout << "Sending info response: " << resp.dump() << std::endl;
            auto payload = nlohmann::json::to_cbor(resp);
            nabto_device_coap_response_set_code(coap, 205);
            nabto_device_coap_response_set_content_format(coap, NABTO_DEVICE_COAP_CONTENT_FORMAT_APPLICATION_CBOR);
            nabto_device_coap_response_set_payload(coap, payload.data(), payload.size());
            nabto_device_coap_response_ready(coap);
        }
        else {
            std::cout << "Got CoAP info request but IAM rejected it" << std::endl;
            nabto_device_coap_error_response(coap, 401, NULL);
        }
        nabto_device_coap_request_free(coap);

    });

    return true;
}


bool SignalingStreamManager::connectionAddMedias(NabtoDeviceConnectionRef ref, std::vector<MediaTrackPtr>& tracks)
{
    SignalingStreamPtr stream = nullptr;

    for (auto p : streams_) {
        auto ptr = p.lock();
        if (ptr && ptr->isConnection(ref)) {
            ptr->createTracks(tracks);
            return true;
        }
    }
    return false;
}

void SignalingStreamManager::setTrackEventCallback(TrackEventCallback cb)
{
    trackCb_ = cb;
}

void SignalingStreamManager::setCheckAccessCallback(CheckAccessCallback cb)
{
    accessCb_ = cb;
}

} // namespace
