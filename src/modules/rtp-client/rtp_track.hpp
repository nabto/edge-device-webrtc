
#pragma once

#include <nabto/nabto_device_webrtc.hpp>
#include <rtc/rtc.hpp>
#include <track-negotiators/track_negotiator.hpp>
#include <rtp-repacketizer/rtp_repacketizer.hpp>

#include <memory>

namespace nabto {

class RtpTrack
{
public:
    MediaTrackPtr track;
    RtpRepacketizerPtr repacketizer;
    rtc::SSRC ssrc;
    int srcPayloadType = 0;
    int dstPayloadType = 0;
};

} // namespace
