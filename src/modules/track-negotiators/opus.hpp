#pragma once

#include "track_negotiator.hpp"

namespace nabto {

class OpusNegotiator : public RtpCodec
{
public:
    static RtpCodecPtr create() { return std::make_shared<OpusNegotiator>(); }
    // The ssrc must be unique in the entire SDP context.
    // This supports both sending and receiving
    OpusNegotiator() : RtpCodec(111, SEND_RECV) {}
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
};

} // namespace
