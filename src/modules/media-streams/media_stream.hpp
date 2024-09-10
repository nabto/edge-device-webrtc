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



    virtual bool isTrack(const std::string& trackId) = 0;
    virtual void addConnection(const std::string& webrtcConnectionId, MediaTrackPtr media) = 0;
    virtual void removeConnection(const std::string& webrtcConnectionId) = 0;

    virtual bool matchMedia(MediaTrackPtr media) = 0;

    virtual MediaTrackPtr createMedia(const std::string& trackId) = 0;
};


} // namespace
