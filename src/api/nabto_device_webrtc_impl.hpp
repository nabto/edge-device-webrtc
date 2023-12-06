#pragma once

#include <nabto/nabto_device_webrtc.hpp>
#include <signaling-stream/signaling_stream_manager.hpp>

#include <memory>

namespace nabto {

class NabtoDeviceWebrtcImpl {
public:
    NabtoDeviceWebrtcImpl(EventQueuePtr queue, NabtoDevicePtr device);
    ~NabtoDeviceWebrtcImpl();

    void start();
    void stop();
    bool connectionAddMedias(NabtoDeviceConnectionRef ref, std::vector<MediaTrackPtr>& tracks);
    void setTrackEventCallback(TrackEventCallback cb);
    void setCheckAccessCallback(CheckAccessCallback cb);

private:
    EventQueuePtr queue_;
    NabtoDevicePtr device_;
    SignalingStreamManagerPtr ssm_;

};

} // namespace nabto
