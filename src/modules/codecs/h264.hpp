#pragma once

#include "rtp_codec.hpp"

namespace nabto {

class H264CodecMatcher : public RtpCodec
{
public:
    // The ssrc must be unique in the entire SDP context.
    // This supports both sending and receiving
    H264CodecMatcher() : RtpCodec(96, SEND_RECV) { }
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
};

} // namespace
