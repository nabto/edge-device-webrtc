
#include "h264.hpp"

#include <rtc/h264rtppacketizer.hpp>
#include <rtc/h264rtpdepacketizer.hpp>

namespace nabto {

class H264Repacketizer: public Repacketizer
{
public:
    H264Repacketizer(MediaTrackPtr track, std::shared_ptr<rtc::RtpPacketizationConfig> rtpConf) : Repacketizer(track, rtpConf->ssrc, rtpConf->payloadType), rtpConf_(rtpConf)
    {
        packet_ = std::make_shared<rtc::H264RtpPacketizer>(rtc::NalUnit::Separator::LongStartSequence, rtpConf_);

    }

    void handlePacket(uint8_t* buffer, size_t length) override
    {
        auto src = reinterpret_cast<const std::byte*>(buffer);
        rtc::message_ptr msg = std::make_shared<rtc::Message>(src, src + length);

        rtc::message_vector vec;
        vec.push_back(msg);

        depacket_.incoming(vec, nullptr);

        if (vec.size() > 0) {
            rtpConf_->timestamp = vec[0]->frameInfo->timestamp;
            packet_->outgoing(vec, nullptr);
            for (auto m : vec) {
                auto variant = rtc::to_variant(*m.get());
                auto bin = std::get<rtc::binary>(variant);
                uint8_t* buf = reinterpret_cast<uint8_t*>(bin.data());
                track_->send(buf, bin.size());
            }
        }
    }
private:
    rtc::H264RtpDepacketizer depacket_;
    std::shared_ptr<rtc::H264RtpPacketizer> packet_ = nullptr;
    std::shared_ptr<rtc::RtpPacketizationConfig> rtpConf_ = nullptr;

};

RepacketizerPtr H264Negotiator::createPacketizer(MediaTrackPtr track, rtc::SSRC ssrc, int dstPayloadType)
{
    if (repacketize_) {
        auto rtpConf = std::make_shared<rtc::RtpPacketizationConfig>(ssrc, track->getTrackId(), dstPayloadType, 90000);

        return std::make_shared<H264Repacketizer>(track, rtpConf);
    } else {
        return std::make_shared<Repacketizer>(track, ssrc, dstPayloadType);
    }
}

int H264Negotiator::match(MediaTrackPtr track)
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
    bool found = false;
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
                NPLOGD << "FOUND RTP BETTER codec match!!! " << pt;
            }
            else if (found) {
                NPLOGD << "h264 pt: " << pt << " no match, removing";
                media.removeRtpMap(pt);
                continue;
            }
            else {
                NPLOGD << "FOUND RTP codec match!!! " << pt;
            }
            found = true; // found a match, just remove any remaining rtpMaps
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
