#pragma once

#include "track_negotiator.hpp"

namespace nabto {

class OpusNegotiator : public TrackNegotiator
{
public:
    static TrackNegotiatorPtr create() { return std::make_shared<OpusNegotiator>(); }
    // The ssrc must be unique in the entire SDP context.
    // This supports both sending and receiving
    OpusNegotiator() : TrackNegotiator(111, SEND_RECV) {}
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
};

} // namespace
