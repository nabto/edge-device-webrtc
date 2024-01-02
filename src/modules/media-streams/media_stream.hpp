#pragma once

#include <nabto/nabto_device.h>

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

    // TODO: remove when main legacy coap stops using it
    virtual std::string getTrackId() = 0;
    // TODO: replace this with: MediaTrackPtr createMedia(std::string& trackId)
    virtual std::string sdp() = 0;
};


} // namespace
