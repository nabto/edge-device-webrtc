#pragma once

#include <nabto-device/nabto_device.hpp>
#include <signaling-stream/signaling_stream.hpp>
#include <media-streams/media_stream.hpp>
#include <nabto/nabto_device_webrtc.hpp>

#include <memory>

namespace nabto {

class SignalingStreamManager;

typedef std::shared_ptr<SignalingStreamManager> SignalingStreamManagerPtr;

class SignalingStreamManager : public std::enable_shared_from_this<SignalingStreamManager>
{
public:
    static SignalingStreamManagerPtr create(NabtoDevicePtr device, EventQueuePtr queue);
    SignalingStreamManager(NabtoDevicePtr device, EventQueuePtr queue);
    ~SignalingStreamManager();

    bool start();
    bool connectionAddMedias(NabtoDeviceConnectionRef ref, std::vector<MediaTrackPtr>& tracks);
    void setTrackEventCallback(TrackEventCallback cb);
    void setCheckAccessCallback(CheckAccessCallback cb);

private:
    void handleVideoRequest(NabtoDeviceCoapRequest* coap);

    NabtoDevicePtr device_;
    EventQueuePtr queue_;

    TrackEventCallback trackCb_;
    CheckAccessCallback accessCb_;

    NabtoDeviceCoapListenerPtr coapInfoListener_ = nullptr;
    NabtoDeviceCoapListenerPtr coapVideoListener_ = nullptr;

    NabtoDeviceStreamListenerPtr streamListener_;

    std::vector<SignalingStreamWeakPtr> streams_;
    SignalingStreamManagerPtr me_ = nullptr;

};


} // namespace
