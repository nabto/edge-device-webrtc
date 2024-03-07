#pragma once

#include "track_negotiator.hpp"

namespace nabto {

class H264Negotiator : public TrackNegotiator
{
public:
    static TrackNegotiatorPtr create() { return std::make_shared<H264Negotiator>(); }
    // The ssrc must be unique in the entire SDP context.
    // This supports both sending and receiving
    H264Negotiator() : TrackNegotiator(96, SEND_RECV) { }
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
};

} // namespace
