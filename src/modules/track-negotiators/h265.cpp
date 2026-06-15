
#include "h265.hpp"

namespace nabto {

int H265Negotiator::match(MediaTrackPtr track)
{
    // We must go through all codecs in the Media Description and remove all codecs other than the one we support.
    // We start by getting and parsing the SDP
    auto sdp = track->getSdp();
    NPLOGD << "    Got offer SDP: " << sdp;
    rtc::Description::Media media(sdp);

    rtc::Description::Media::RtpMap* rtp = NULL;
    // Loop all payload types offered by the client
    for (auto pt : media.payloadTypes()) {
        rtc::Description::Media::RtpMap* r = NULL;
        try {
            // Get the RTP description for this payload type
            r = media.rtpMap(pt);
        } catch (std::exception& ex) {
            // Since we are getting the description based on the list of payload types this should never fail, but just in case.
            NPLOGE << "Bad rtpMap for pt: " << pt;
            continue;
        }
        // If this payload type is H265/90000 we found a match.
        // libdatachannel emits the format string as "H265" (uppercase) - see
        // Description::Video::addH265Codec in libdatachannel's description.cpp.
        if (r->format == "H265" && r->clockRate == 90000) {
            if (rtp != NULL) {
                // We already chose one; drop this duplicate.
                NPLOGD << "h265 pt: " << pt << " duplicate, removing";
                media.removeRtpMap(pt);
                continue;
            }
            NPLOGD << "FOUND RTP codec match!!! " << pt;
            rtp = r;
            NPLOGD << "Format: " << rtp->format << " clockRate: " << rtp->clockRate << " encParams: " << rtp->encParams;
            NPLOGD << "rtcp fbs:";
            for (auto s : rtp->rtcpFbs) {
                NPLOGD << "   " << s;
            }

            // Our implementation does not support these feedback extensions, so we remove them (if they exist)
            // Though the technically correct way to do it, trial and error has shown this has no practial effect.
            rtp->removeFeedback("nack");
            rtp->removeFeedback("goog-remb");
            rtp->removeFeedback("transport-cc");
            rtp->removeFeedback("ccm fir");
        }
        else {
            // We remove any payload type not matching our codec
            NPLOGD << "pt: " << pt << " no match, removing";
            media.removeRtpMap(pt);
        }
    }
    if (rtp == NULL) {
        return 0;
    }
    // Add the ssrc to the track
    auto trackId = track->getTrackId();
    media.addSSRC(ssrc(), trackId);
    // Generate the SDP string of the updated Media Description
    auto newSdp = media.generateSdp();
    NPLOGD << "    Setting new SDP: " << newSdp;
    // and set the new SDP on the track
    track->setSdp(newSdp);
    return rtp->payloadType;
}

rtc::Description::Media H265Negotiator::createMedia()
{
    // Create a Video media description.
    // We support both sending and receiving video
    std::string mid = MidGenerator::generateMid();
    rtc::Description::Video media(mid, rtc::Description::Direction::SendRecv);

    // Since we are creating the media track, only the supported payload type exists, so we might as well reuse the same value for the RTP session in WebRTC as the one we use in the RTP source (eg. Gstreamer)
    media.addH265Codec(payloadType_);

    // Remove unsupported feedback extensions to match what match() advertises.
    auto r = media.rtpMap(payloadType_);
    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
}

} // namespace
