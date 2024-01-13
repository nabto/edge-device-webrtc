#pragma once

#include "rtp_codec.hpp"

namespace nabto {

class PcmuCodecMatcher : public RtpCodec
{
public:
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
    int payloadType() { return 0; }
    int ssrc() { return 44; }
    enum Direction direction() { return SEND_ONLY; }
};


} // namespace
