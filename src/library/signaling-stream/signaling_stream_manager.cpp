#include "signaling_stream_manager.hpp"

#include <nlohmann/json.hpp>

namespace nabto {

const char* coapInfoPath[] = { "p2p", "webrtc-info", NULL };

SignalingStreamManagerPtr SignalingStreamManager::create(NabtoDevicePtr device, EventQueuePtr queue)
{
    return std::make_shared<SignalingStreamManager>(device, queue);
}

SignalingStreamManager::SignalingStreamManager(NabtoDevicePtr device, EventQueuePtr queue) : device_(device), queue_(queue)
{
    streamListener_ = NabtoStreamListener::create(device_, queue_);
    streamListenerV2_ = NabtoStreamListener::create(device_, queue_);
    coapInfoListener_ = NabtoCoapListener::create(device_, NABTO_DEVICE_COAP_GET, coapInfoPath, queue_);
}

SignalingStreamManager::~SignalingStreamManager()
{
}

bool SignalingStreamManager::start()
{
    auto self = shared_from_this();
    streamListener_->setStreamCallback([self](NabtoDeviceStream* stream) {
        NabtoDeviceConnectionRef ref = nabto_device_stream_get_connection_ref(stream);
        if (self->accessCb_ && self->accessCb_(ref, "Webrtc:Signaling"))
        {
            char* fp = NULL;
            nabto_device_connection_get_client_fingerprint(self->device_.get(), ref, &fp);
            NPLOGD << "Creating Signaling stream for client fp: " << (fp == NULL ? "NO FP" : fp);
            nabto_device_string_free(fp);
            SignalingStreamPtr s = SignalingStream::create(self->device_, stream, self, self->queue_,
                [self](NabtoDeviceConnectionRef connRef, MediaTrackPtr track) {
                    self->trackCb_(connRef, track);
                },
                [self](NabtoDeviceConnectionRef ref, std::string action) -> bool {
                    return self->accessCb_(ref, action);
                },
                [self](NabtoDeviceConnectionRef connRef, DatachannelPtr channel) {
                    self->datachannelCb_(connRef, channel);
                }, false);
            self->streams_.push_back(s);
            s->start();
        }
        else {
            NPLOGI << "New signaling stream opened, but IAM rejected it";
            nabto_device_stream_free(stream);
        }
    });

    streamListenerV2_->setStreamCallback([self](NabtoDeviceStream* stream) {
        NabtoDeviceConnectionRef ref = nabto_device_stream_get_connection_ref(stream);
        if (self->accessCb_ && self->accessCb_(ref, "Webrtc:Signaling"))
        {
            char* fp = NULL;
            nabto_device_connection_get_client_fingerprint(self->device_.get(), ref, &fp);
            NPLOGD << "Creating V2 Signaling stream for client fp: " << (fp == NULL ? "NO FP" : fp);
            nabto_device_string_free(fp);
            SignalingStreamPtr s = SignalingStream::create(self->device_, stream, self, self->queue_,
                [self](NabtoDeviceConnectionRef connRef, MediaTrackPtr track) {
                    self->trackCb_(connRef, track);
                },
                [self](NabtoDeviceConnectionRef ref, std::string action) -> bool {
                    return self->accessCb_(ref, action);
                },
                [self](NabtoDeviceConnectionRef connRef, DatachannelPtr channel) {
                    self->datachannelCb_(connRef, channel);
                }, true);
            self->streams_.push_back(s);
            s->start();
        }
        else {
            NPLOGI << "New V2 signaling stream opened, but IAM rejected it";
            nabto_device_stream_free(stream);
        }
        });

    coapInfoListener_->setCoapCallback([self](NabtoDeviceCoapRequest* coap) {
        NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

        if (self->accessCb_ && self->accessCb_(ref, "Webrtc:GetInfo"))
        {
            nlohmann::json resp = {
                {"SignalingStreamPort", self->streamListener_->getStreamPort()},
                { "SignalingV2StreamPort", self->streamListenerV2_->getStreamPort() }
            };
            NPLOGD << "Sending info response: " << resp.dump();
            auto payload = resp.dump();
            nabto_device_coap_response_set_code(coap, 205);
            nabto_device_coap_response_set_content_format(coap, NABTO_DEVICE_COAP_CONTENT_FORMAT_APPLICATION_JSON);
            nabto_device_coap_response_set_payload(coap, payload.data(), payload.size());
            nabto_device_coap_response_ready(coap);
        }
        else {
            NPLOGI << "Got CoAP info request but IAM rejected it";
            nabto_device_coap_error_response(coap, 401, NULL);
        }
        nabto_device_coap_request_free(coap);

    });

    return true;
}


bool SignalingStreamManager::connectionAddMediaTracks(NabtoDeviceConnectionRef ref, const std::vector<MediaTrackPtr>& tracks)
{
    SignalingStreamPtr stream = nullptr;

    for (auto p : streams_) {
        auto ptr = p.lock();
        if (ptr && ptr->isConnection(ref)) {
            return ptr->createTracks(tracks);
        }
    }
    return false;
}

void SignalingStreamManager::setTrackEventCallback(TrackEventCallback cb)
{
    trackCb_ = cb;
}

void SignalingStreamManager::setDatachannelEventCallback(DatachannelEventCallback cb)
{
    datachannelCb_ = cb;
}

void SignalingStreamManager::setCheckAccessCallback(CheckAccessCallback cb)
{
    accessCb_ = cb;
}

} // namespace
