#pragma once

#include "rtp_codec.hpp"

namespace nabto {

class OpusCodecMatcher : public RtpCodec
{
public:
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
    int payloadType() { return 111; }
    int ssrc() { return 43; }
    enum Direction direction() { return SEND_RECV; }
};

} // namespace
