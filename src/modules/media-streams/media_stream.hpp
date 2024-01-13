#pragma once

#include <nabto/nabto_device.h>
#include <nabto/nabto_device_webrtc.hpp>

#include <rtc/rtc.hpp>

#include <memory>
#include <functional>

namespace nabto {

class MediaStream;
typedef std::shared_ptr<MediaStream> MediaStreamPtr;

class MediaStream {
public:
    MediaStream() {}
    ~MediaStream() {}



    virtual bool isTrack(std::string& trackId) = 0;
    virtual void addConnection(NabtoDeviceConnectionRef ref, MediaTrackPtr media) = 0;
    virtual void removeConnection(NabtoDeviceConnectionRef ref) = 0;

    virtual bool matchMedia(MediaTrackPtr media) = 0;

    virtual MediaTrackPtr createMedia(std::string& trackId) = 0;
};


} // namespace
