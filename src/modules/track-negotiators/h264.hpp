#pragma once

#include "track_negotiator.hpp"

namespace nabto {

class H264Negotiator : public TrackNegotiator
{
public:
    static TrackNegotiatorPtr create(bool repacketize = true) { return std::make_shared<H264Negotiator>(repacketize); }
    // The ssrc must be unique in the entire SDP context.
    // This supports both sending and receiving
    H264Negotiator(bool repacketize) : TrackNegotiator(96, SEND_RECV), repacketize_(repacketize) { }
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
    RepacketizerPtr createPacketizer(MediaTrackPtr track);
private:
    bool repacketize_;
};

} // namespace
