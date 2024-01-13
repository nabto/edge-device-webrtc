
#include "pcmu.hpp"

namespace nabto {

int PcmuCodecMatcher::match(MediaTrackPtr track)
{
    auto sdp = track->getSdp();
    std::cout << "    Got offer SDP: " << sdp << std::endl;
    // TODO: remove when updating libdatachannel after https://github.com/paullouisageneau/libdatachannel/issues/1074
    if (sdp[0] == 'm' && sdp[1] == '=') {
        sdp = sdp.substr(2);
    }
    rtc::Description::Media media(sdp);

    rtc::Description::Media::RtpMap* rtp = NULL;
    for (auto pt : media.payloadTypes()) {
        rtc::Description::Media::RtpMap* r = NULL;
        try {
            r = media.rtpMap(pt);
        } catch (std::exception& ex) {
            std::cout << "Bad rtpMap for pt: " << pt << std::endl;
            continue;
        }
        if (r != NULL && r->format == "pcmu" && r->clockRate == 8000) {
            std::cout << "Found RTP codec for audio! pt: " << r->payloadType << std::endl;
            rtp = r;
            rtp->removeFeedback("nack");
            rtp->removeFeedback("goog-remb");
            rtp->removeFeedback("transport-cc");
            rtp->removeFeedback("ccm fir");
        }
        else {
            // std::cout << "no match, removing" << std::endl;
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

rtc::Description::Media PcmuCodecMatcher::createMedia()
{
    rtc::Description::Audio media("audio", rtc::Description::Direction::SendRecv);
    media.addPCMUCodec(0);
    auto r = media.rtpMap(0);
    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
}


} // namespace
