#pragma once

#include "track_negotiator.hpp"

namespace nabto {

class PcmuNegotiator : public RtpCodec
{
public:
    static RtpCodecPtr create() { return std::make_shared<PcmuNegotiator>(); }
    // The ssrc must be unique in the entire SDP context.
    // This supports both sending and receiving
    PcmuNegotiator() : RtpCodec(0, SEND_RECV) {}
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
};


} // namespace
