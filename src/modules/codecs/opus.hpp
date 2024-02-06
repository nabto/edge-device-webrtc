#pragma once

#include "rtp_codec.hpp"

namespace nabto {

class OpusCodecMatcher : public RtpCodec
{
public:
    // The ssrc must be unique in the entire SDP context.
    // This supports both sending and receiving
    OpusCodecMatcher() : RtpCodec(111, SEND_RECV) {}
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
};

} // namespace
