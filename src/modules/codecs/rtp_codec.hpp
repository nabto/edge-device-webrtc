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

    // payload type used to create media
    virtual int payloadType() = 0;

    // SSRC of the media
    virtual int ssrc() = 0;

    // Direction of the created media
    virtual enum Direction direction() { return SEND_ONLY; }
};

} // namespace
