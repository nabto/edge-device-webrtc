#include "signaling_stream_manager.hpp"

#include <nlohmann/json.hpp>

namespace nabto {

const char* coapInfoPath[] = { "webrtc", "info", NULL };
const char* coapVideoPath[] = { "webrtc", "video", "{feed}", NULL };

SignalingStreamManagerPtr SignalingStreamManager::create(NabtoDeviceImplPtr device, std::vector<nabto::MediaStreamPtr>& medias, EventQueuePtr queue)
{
    return std::make_shared<SignalingStreamManager>(device, medias, queue);
}

SignalingStreamManager::SignalingStreamManager(NabtoDeviceImplPtr device, std::vector<nabto::MediaStreamPtr>& medias, EventQueuePtr queue) : device_(device), medias_(medias), queue_(queue)
{
    streamListener_ = NabtoDeviceStreamListener::create(device_, queue_);
    coapInfoListener_ = NabtoDeviceCoapListener::create(device_, NABTO_DEVICE_COAP_GET, coapInfoPath, queue_);
    coapVideoListener_ = NabtoDeviceCoapListener::create(device_, NABTO_DEVICE_COAP_GET, coapVideoPath, queue_);
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
        if (nm_iam_check_access(self->device_->getIam(), ref, "Webrtc:Signaling", NULL))
        {
            char* fp;
            nabto_device_connection_get_client_fingerprint(self->device_->getDevice(), ref, &fp);
            std::cout << "Creating Signaling stream for client fp: " << fp << std::endl;
            nabto_device_string_free(fp);
            SignalingStreamPtr s = SignalingStream::create(self->device_, stream, self, self->medias_, self->queue_);
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

        if (nm_iam_check_access(self->device_->getIam(), ref, "Webrtc:GetInfo", NULL))
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
            std::cout << "Got CoAP info request but IAM rejected it" << std::endl;
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
    NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

    if (nm_iam_check_access(device_->getIam(), ref, "Webrtc:VideoStream", NULL)) {
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
                    nabto_device_coap_error_response(coap, 500, "Internal Server Error");
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
        bool pwd = nabto_device_connection_is_password_authenticated(device_->getDevice(), ref);
        char* username;

        if (pwd) {
            nabto_device_connection_get_password_authentication_username(device_->getDevice(), ref, &username);
        }

        std::cout << "Got CoAP video stream request but IAM rejected it" << std::endl << "  was pwd: " << pwd << std::endl << "  username: " << (pwd ? username : "") << std::endl;
        if (pwd) {
            nabto_device_string_free(username);
        }
        nabto_device_coap_error_response(coap, 401, NULL);
    }
}


} // namespace
