
#include "h264.hpp"

namespace nabto {

int H264CodecMatcher::match(MediaTrackPtr track)
{
    auto sdp = track->getSdp();
    std::cout << "    Got offer SDP: " << sdp << std::endl;
    // TODO: remove when updating libdatachannel after https://github.com/paullouisageneau/libdatachannel/issues/1074
    if (sdp[0] == 'm' && sdp[1] == '=') {
        sdp = sdp.substr(2);
    }
    rtc::Description::Media media(sdp);

    rtc::Description::Media::RtpMap* rtp = NULL;
    bool found = false;
    for (auto pt : media.payloadTypes()) {
        rtc::Description::Media::RtpMap* r = NULL;
        try {
            r = media.rtpMap(pt);
        } catch (std::exception& ex) {
            std::cout << "Bad rtpMap for pt: " << pt << std::endl;
            continue;
        }
        if (r->format == "H264" && r->clockRate == 90000) {
            std::string profLvlId = "42e01f";
            std::string lvlAsymAllowed = "1";
            std::string pktMode = "1";

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
            rtp->removeFeedback("nack");
            rtp->removeFeedback("goog-remb");
            rtp->removeFeedback("transport-cc");
            rtp->removeFeedback("ccm fir");
        }
        else {
            std::cout << "pt: " << pt << " no match, removing" << std::endl;
            media.removeRtpMap(pt);
        }
    }
    if (rtp == NULL) {
        return 0;
    }
    auto trackId = track->getTrackId();
    media.addSSRC(ssrc(), trackId);
    auto newSdp = media.generateSdp();
    std::cout << "    Setting new SDP: " << newSdp << std::endl;
    track->setSdp(newSdp);
    return rtp->payloadType;
}

rtc::Description::Media H264CodecMatcher::createMedia()
{
    rtc::Description::Video media("video", rtc::Description::Direction::SendOnly);
    media.addH264Codec(96);

    auto r = media.rtpMap(96);
    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
}


} // namespace
