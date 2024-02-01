#pragma once

#include "rtp_codec.hpp"

namespace nabto {

class H264CodecMatcher : public RtpCodec
{
public:
    int match(MediaTrackPtr media);
    rtc::Description::Media createMedia();
    int payloadType() { return payloadType_; }
    int ssrc() { return ssrc_; }
    enum Direction direction() {
        // This supports both sending and receiving
        return SEND_RECV;
    }
private:
    // The payload type used by the RTP source (eg. Gstreamer)
    int payloadType_ = 96;
    // The ssrc must be unique in the entire SDP context. We only support one video feed so any constant different from other codec matchers is fine.
    int ssrc_ = 42;
};

} // namespace
