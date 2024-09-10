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
    bool connectionAddMediaTracks(NabtoDeviceConnectionRef ref, const std::vector<MediaTrackPtr>& tracks);
    void setTrackEventCallback(TrackEventCallback cb);
    void setDatachannelEventCallback(DatachannelEventCallback cb);
    void setMetadataEventCallback(MetadataEventCallback cb);
    bool connectionSendMetadata(std::string id, std::string metadata);
    NabtoDeviceConnectionRef getNabtoConnectionRef(std::string webrtcConnectionId);
    void setCheckAccessCallback(CheckAccessCallback cb);

private:
    EventQueuePtr queue_;
    NabtoDevicePtr device_;
    SignalingStreamManagerPtr ssm_;
    TrackEventCallback trackCb_;
    CheckAccessCallback accessCb_;

};

} // namespace nabto
