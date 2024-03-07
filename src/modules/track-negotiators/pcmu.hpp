#pragma once

#include "rtp_codec.hpp"

namespace nabto {

class PcmuCodecMatcher : public RtpCodec
{
public:
    static RtpCodecPtr create() { return std::make_shared<PcmuCodecMatcher>(); }
    // The ssrc must be unique in the entire SDP context.
    // This supports both sending and receiving
    PcmuCodecMatcher() : RtpCodec(0, SEND_RECV) {}
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
};


} // namespace
