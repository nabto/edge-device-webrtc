#pragma once

#include "rtp_codec.hpp"

namespace nabto {

class PcmuCodecMatcher : public RtpCodec
{
public:
    // The ssrc must be unique in the entire SDP context. We only support one audio feed so any constant different from other codec matchers is fine.
    // This supports both sending and receiving
    PcmuCodecMatcher() : RtpCodec(0, 44, SEND_RECV) {}
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
};


} // namespace
