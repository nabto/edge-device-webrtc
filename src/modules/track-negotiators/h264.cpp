
#include "h264.hpp"

namespace nabto {

int H264Negotiator::match(MediaTrackPtr track)
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
    bool found = false;
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
        // If this payload type is H264/90000
        if (r->format == "H264" && r->clockRate == 90000) {
            // We also want the codec to match:
            // level-asymmetry-allowed=1
            // packetization-mode=1
            // profile-level-id=42e01f
            std::string profLvlId = "42e01f";
            std::string lvlAsymAllowed = "1";
            std::string pktMode = "1";

            // However, through trial and error, these does not always have to match perfectly, so for a bit of flexibility we allow any H264 codec.
            // If a later codec matches perfectly we update our choice.
            if (found && r->fmtps.size() > 0 &&
                r->fmtps[0].find("profile-level-id=" + profLvlId) != std::string::npos &&
                r->fmtps[0].find("level-asymmetry-allowed=" + lvlAsymAllowed) != std::string::npos &&
                r->fmtps[0].find("packetization-mode=" + pktMode) != std::string::npos
                ) {
                // Found better match use this
                media.removeRtpMap(rtp->payloadType);
                rtp = r;
                std::cout << "FOUND RTP BETTER codec match!!! " << pt << std::endl;
            }
            else if (found) {
                std::cout << "h264 pt: " << pt << " no match, removing" << std::endl;
                media.removeRtpMap(pt);
                continue;
            }
            else {
                std::cout << "FOUND RTP codec match!!! " << pt << std::endl;
            }
            found = true; // found a match, just remove any remaining rtpMaps
            rtp = r;
            std::cout << "Format: " << rtp->format << " clockRate: " << rtp->clockRate << " encParams: " << rtp->encParams << std::endl;
            std::cout << "rtcp fbs:" << std::endl;
            for (auto s : rtp->rtcpFbs) {
                std::cout << "   " << s << std::endl;
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
            std::cout << "pt: " << pt << " no match, removing" << std::endl;
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

rtc::Description::Media H264Negotiator::createMedia()
{
    // Create a Video media description.
    // We support both sending and receiving video
    std::string mid = MidGenerator::generateMid();
    rtc::Description::Video media(mid, rtc::Description::Direction::SendRecv);

    // Since we are creating the media track, only the supported payload type exists, so we might as well reuse the same value for the RTP session in WebRTC as the one we use in the RTP source (eg. Gstreamer)
    media.addH264Codec(payloadType_);

    // Libdatachannel H264 default codec is already using:
    // level-asymmetry-allowed=1
    // packetization-mode=1
    // profile-level-id=42e01f
    // However, again to be technically correct, we remove the unsupported feedback extensions
    auto r = media.rtpMap(payloadType_);
    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
}


} // namespace
