
#pragma once

#include <nabto/nabto_device_webrtc.hpp>
#include <rtc/rtc.hpp>

#include <memory>

namespace nabto {

class RtpTrack
{
public:
    MediaTrackPtr track;
    rtc::SSRC ssrc;
    int srcPayloadType = 0;
    int dstPayloadType = 0;
};

} // namespace
