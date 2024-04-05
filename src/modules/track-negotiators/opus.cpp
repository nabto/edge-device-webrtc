#include "opus.hpp"

namespace nabto {

int OpusNegotiator::match(MediaTrackPtr track)
{
    // We must go through all codecs in the Media Description and remove all codecs other than the one we support.
    // We start by getting and parsing the SDP
    auto sdp = track->getSdp();
    NPLOGD << "    Got offer SDP: " << sdp;
    // TODO: remove when updating libdatachannel after https://github.com/paullouisageneau/libdatachannel/issues/1074
    if (sdp[0] == 'm' && sdp[1] == '=') {
        sdp = sdp.substr(2);
    }
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
        // If this payload type is opus/48000 we found a match
        if (r != NULL && r->format == "opus" && r->clockRate == 48000) {
            NPLOGD << "Found RTP codec for audio! pt: " << r->payloadType;
            // Our implementation does not support these feedback extensions, so we remove them (if they exist)
            // Though the technically correct way to do it, trial and error has shown this has no practial effect.
            rtp = r;
            rtp->removeFeedback("nack");
            rtp->removeFeedback("goog-remb");
            rtp->removeFeedback("transport-cc");
            rtp->removeFeedback("ccm fir");
        }
        else {
            // We remove any payload type not matching our codec
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

rtc::Description::Media OpusNegotiator::createMedia()
{
    // Create an Audio media description.
    // We support both sending and receiving audio
    const std::string mid = MidGenerator::generateMid();
    rtc::Description::Audio media(mid, rtc::Description::Direction::SendRecv);

    // Since we are creating the media track, only the supported payload type exists, so we might as well reuse the same value for the RTP session in WebRTC as the one we use in the RTP source (eg. Gstreamer)
    media.addOpusCodec(payloadType_);
    auto r = media.rtpMap(payloadType_);
    // Again to be technically correct, we remove the unsupported feedback extensions
    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
}


} // namespace
