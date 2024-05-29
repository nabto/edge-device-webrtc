#pragma once

#include <nabto-listeners/nabto_listeners.hpp>
#include <signaling-stream/signaling_stream.hpp>
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
    bool connectionAddMediaTracks(NabtoDeviceConnectionRef ref, const std::vector<MediaTrackPtr>& tracks);
    void setTrackEventCallback(TrackEventCallback cb);
    void setCheckAccessCallback(CheckAccessCallback cb);

private:
    NabtoDevicePtr device_;
    EventQueuePtr queue_;

    TrackEventCallback trackCb_;
    CheckAccessCallback accessCb_;

    NabtoCoapListenerPtr coapInfoListener_ = nullptr;

    NabtoStreamListenerPtr streamListener_;

    std::vector<SignalingStreamWeakPtr> streams_;
    SignalingStreamManagerPtr me_ = nullptr;

};


} // namespace
