#pragma once

#include <nabto-device/nabto_device.hpp>
#include <signaling-stream/signaling_stream.hpp>
#include <media-streams/media_stream.hpp>
#include <event-queue/event_queue.hpp>

#include <memory>

namespace nabto {

class SignalingStreamManager;

typedef std::shared_ptr<SignalingStreamManager> SignalingStreamManagerPtr;

class SignalingStreamManager : public std::enable_shared_from_this<SignalingStreamManager>
{
public:
    static SignalingStreamManagerPtr create(NabtoDeviceImplPtr device, std::vector<nabto::MediaStreamPtr>& medias, EventQueuePtr queue);
    SignalingStreamManager(NabtoDeviceImplPtr device, std::vector<nabto::MediaStreamPtr>& medias, EventQueuePtr queue);
    ~SignalingStreamManager();

    bool start();

private:
    void handleVideoRequest(NabtoDeviceCoapRequest* coap);

    NabtoDeviceImplPtr device_;
    std::vector<nabto::MediaStreamPtr> medias_;
    EventQueuePtr queue_;

    NabtoDeviceCoapListenerPtr coapInfoListener_ = nullptr;

    NabtoDeviceCoapListenerPtr coapVideoListener_ = nullptr;

    NabtoDeviceStreamListenerPtr streamListener_;

    std::vector<SignalingStreamWeakPtr> streams_;
    SignalingStreamManagerPtr me_ = nullptr;

};


} // namespace
