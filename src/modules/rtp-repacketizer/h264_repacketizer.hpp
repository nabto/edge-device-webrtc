#pragma once

#include "rtp_repacketizer.hpp"

#include <rtc/h264rtppacketizer.hpp>
#include <rtc/h264rtpdepacketizer.hpp>


namespace nabto {

class H264Repacketizer;
typedef std::shared_ptr<H264Repacketizer> H264RepacketizerPtr;


class H264Repacketizer : public RtpRepacketizer
{
public:
    static RtpRepacketizerPtr create(MediaTrackPtr track, std::shared_ptr<rtc::RtpPacketizationConfig> rtpConf) { return std::make_shared<H264Repacketizer>(track, rtpConf); }


    H264Repacketizer(MediaTrackPtr track, std::shared_ptr<rtc::RtpPacketizationConfig> rtpConf) : RtpRepacketizer(track, rtpConf->ssrc, rtpConf->payloadType), rtpConf_(rtpConf)
    {
        packet_ = std::make_shared<rtc::H264RtpPacketizer>(rtc::NalUnit::Separator::LongStartSequence, rtpConf_);

    }

    void handlePacket(uint8_t* buffer, size_t length) override
    {
        if (length < 2 || buffer[1] == 200) {
            // ignore RTCP packets for now
            return;
        }
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

class H264RepacketizerFactory : public RtpRepacketizerFactory
{
public:
    static RtpRepacketizerFactoryPtr create() {
        return std::make_shared<H264RepacketizerFactory>();
    }
    H264RepacketizerFactory() { }
    RtpRepacketizerPtr createPacketizer(MediaTrackPtr track, uint32_t ssrc, int dstPayloadType)
    {
        auto rtpConf = std::make_shared<rtc::RtpPacketizationConfig>(ssrc, track->getTrackId(), dstPayloadType, 90000);
        return std::make_shared<H264Repacketizer>(track, rtpConf);
    }
};

} // namespace
