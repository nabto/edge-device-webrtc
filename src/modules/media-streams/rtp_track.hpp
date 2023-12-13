
#pragma once

#include <rtc/rtc.hpp>

#include <memory>

namespace nabto {

typedef std::shared_ptr<rtc::Track> RtcTrackPtr;
typedef std::shared_ptr<rtc::PeerConnection> RtcPCPtr;

class RtpTrack
{
public:
    RtcPCPtr pc;
    RtcTrackPtr track;
    rtc::SSRC ssrc;
    int srcPayloadType = 0;
    int dstPayloadType = 0;
};

} // namespace
