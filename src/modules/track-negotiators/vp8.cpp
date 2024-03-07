#include "vp8.hpp"

namespace nabto {

int VP8CodecMatcher::match(MediaTrackPtr track)
{
    // We must go through all codecs in the Media Description and remove all codecs other than the one we support.
    // We start by getting and parsing the SDP
    auto sdp = track->getSdp();
    std::cout << "    Got offer SDP: " << sdp << std::endl;
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
            std::cout << "Bad rtpMap for pt: " << pt << std::endl;
            continue;
        }
        // If this payload type is vp8/90000 we found a match
        if (r != NULL && r->format == "vp8" && r->clockRate == 90000) {
            std::cout << "Found RTP codec for audio! pt: " << r->payloadType << std::endl;
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
    std::cout << "    Setting new SDP: " << newSdp << std::endl;
    // and set the new SDP on the track
    track->setSdp(newSdp);
    return rtp->payloadType;
}

rtc::Description::Media VP8CodecMatcher::createMedia()
{
    // Create an Audio media description.
    // We support both sending and receiving audio
    const std::string mid = MidGenerator::generateMid();
    rtc::Description::Video media(mid, rtc::Description::Direction::SendRecv);

    // Since we are creating the media track, only the supported payload type exists, so we might as well reuse the same value for the RTP session in WebRTC as the one we use in the RTP source (eg. Gstreamer)
    media.addVP8Codec(payloadType_);
    auto r = media.rtpMap(payloadType_);
    // Again to be technically correct, we remove the unsupported feedback extensions
    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
}


} // namespace
