#pragma once

#include "rtp_codec.hpp"

namespace nabto {

class VP8CodecMatcher : public RtpCodec
{
public:
    static RtpCodecPtr create() { return std::make_shared<VP8CodecMatcher>(); }
    // The ssrc must be unique in the entire SDP context.
    // This supports both sending and receiving
    VP8CodecMatcher() : RtpCodec(127, SEND_RECV) {}
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
};

} // namespace
