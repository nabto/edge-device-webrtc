#pragma once

#include <media-streams/media_stream.hpp>

namespace nabto {

class RtpCodec
{
public:
    enum Direction {
        SEND_ONLY, // Device only sends data
        RECV_ONLY, // Device only receives data
        SEND_RECV  // Device both sends and receives data
    };

    RtpCodec(int pt, int ssrc, enum Direction dire):
        payloadType_(pt),
        ssrc_(ssrc),
        dire_(dire) {}
    /**
     * match takes a media description and loops through the rtp map
     * For each map entry if:
     *    it matches: its payload type is used as return value
     *    it doesn't match: it is removed from the map
     **/
    virtual int match(MediaTrackPtr media) = 0;

    /**
     * createMedia creates a media description of the proper type and returns it.
     * The caller will add ssrc and track ID to the returned description.
     */
    virtual rtc::Description::Media createMedia() = 0;

    /**
     * payload type used to create media. This value should match the
     * payload type of the RTP source (eg. Gstreamer).
     *
     * The payload type used on the WebRTC connection depends on the
     * negotiation. The RTP client module will translate between the
     * two values.
     */
    virtual int payloadType() { return payloadType_;};

    /**
     * SSRC of the media.
     * The SSRC must be unique for each source added across an entire SDP
     * negotiation. To ensure this, it is recommended to use a UUID. For
     * simplicity, the base SSRC is just a static value.
     */
    virtual int ssrc() { return ssrc_; };

    /**
     *  Direction of the created media
     */
    virtual enum Direction direction() { return dire_; }

protected:
    int payloadType_;
    int ssrc_;
    enum Direction dire_;
};

} // namespace
