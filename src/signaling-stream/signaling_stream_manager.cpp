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
    streamListener_ = NabtoDeviceStreamListener::create(device_);
    coapInfoListener_ = NabtoDeviceCoapListener::create(device_, NABTO_DEVICE_COAP_GET, coapInfoPath);
    coapVideoListener_ = NabtoDeviceCoapListener::create(device_, NABTO_DEVICE_COAP_GET, coapVideoPath);
}

SignalingStreamManager::~SignalingStreamManager()
{
    std::cout << "SignalingStreamManager Destructor" << std::endl;
}

bool SignalingStreamManager::start()
{
    auto self = shared_from_this();
    streamListener_->setStreamCallback([self](NabtoDeviceStream* stream) {
        if (true) // TODO: check IAM
        {
            std::cout << "Creating Signaling stream" << std::endl;
            SignalingStreamPtr s = SignalingStream::create(self->device_, stream, self, self->medias_);
            self->streams_.push_back(s);
            s->start();
        }
        else {
            nabto_device_stream_free(stream);
        }
    });

    coapInfoListener_->setCoapCallback([self](NabtoDeviceCoapRequest* coap) {
        if (true) // TODO: check IAM
        {
            nlohmann::json resp = {
                {"SignalingStreamPort", self->streamListener_->getStreamPort()},
                {"FileStreamPort", self->device_->getFileStreamPort()}
            };
            std::cout << "Sending info response: " << resp.dump() << std::endl;
            auto payload = nlohmann::json::to_cbor(resp);
            nabto_device_coap_response_set_code(coap, 205);
            nabto_device_coap_response_set_content_format(coap, NABTO_DEVICE_COAP_CONTENT_FORMAT_APPLICATION_CBOR);
            nabto_device_coap_response_set_payload(coap, payload.data(), payload.size());
            nabto_device_coap_response_ready(coap);
        }
        else {
            nabto_device_coap_error_response(coap, 401, NULL);
        }
        nabto_device_coap_request_free(coap);

    });

    coapVideoListener_->setCoapCallback([self](NabtoDeviceCoapRequest* coap) {
        std::cout << "Got new coap request" << std::endl;
        self->handleVideoRequest(coap);
        std::cout << "Freeing coap request" << std::endl;
        nabto_device_coap_request_free(coap);

    });
    return true;
}

void SignalingStreamManager::handleVideoRequest(NabtoDeviceCoapRequest* coap)
{
    // TODO: check IAM
    if (true) {
        std::cout << "Handling video coap request" << std::endl;
        const char* feedC = nabto_device_coap_request_get_parameter(coap, "feed");
        std::string feed(feedC);
        NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

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
            nabto_device_coap_error_response(coap, 400, "Invalid connection");
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
                    nabto_device_coap_error_response(coap, 400, "Invalid connection");
                    return;
                }
            }
        }

        if (found) {
            std::cout << "found track ID in feeds returning 201" << std::endl;
            nabto_device_coap_response_set_code(coap, 201);
            nabto_device_coap_response_ready(coap);
        } else {
            std::cout << "Failed to find track for video feed" << std::endl;
            nabto_device_coap_error_response(coap, 404, "No such feed");
        }
    } else {
        nabto_device_coap_error_response(coap, 401, NULL);
    }
}


} // namespace
