#pragma once

#include "rtp_codec.hpp"

namespace nabto {

class H264CodecMatcher : public RtpCodec
{
public:
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
    int payloadType() { return 96; }
    int ssrc() { return 42; }
};

} // namespace
