#pragma once

#include "track_negotiator.hpp"

namespace nabto {

class PcmuNegotiator : public TrackNegotiator
{
public:
    static TrackNegotiatorPtr create() { return std::make_shared<PcmuNegotiator>(); }
    // The ssrc must be unique in the entire SDP context.
    // This supports both sending and receiving
    PcmuNegotiator() : TrackNegotiator(0, SEND_RECV) {}
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
};


} // namespace
