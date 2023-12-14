#pragma once

#include "rtp_track.hpp"
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

    virtual void addTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc, std::string trackId) = 0;

    virtual void createTrack(std::shared_ptr<rtc::PeerConnection> pc) = 0;

    virtual std::string getTrackId() = 0;

    virtual void removeConnection(std::shared_ptr<rtc::PeerConnection> pc) = 0;

    virtual void removeConnection(NabtoDeviceConnectionRef ref) = 0;
    virtual void addConnection(NabtoDeviceConnectionRef ref, RtpTrack track) = 0;

    virtual std::string sdp() = 0;
};


} // namespace
