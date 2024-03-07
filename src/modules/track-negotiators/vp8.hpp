#pragma once

#include "track_negotiator.hpp"

namespace nabto {

class VP8Negotiator : public TrackNegotiator
{
public:
    static TrackNegotiatorPtr create() { return std::make_shared<VP8Negotiator>(); }
    // The ssrc must be unique in the entire SDP context.
    // This supports both sending and receiving
    VP8Negotiator() : TrackNegotiator(127, SEND_RECV) {}
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
};

} // namespace
