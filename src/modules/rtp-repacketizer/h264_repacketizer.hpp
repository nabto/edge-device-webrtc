#pragma once

#include "rtp_repacketizer.hpp"

#include <rtc/rtc.hpp>



namespace nabto {

class H264Repacketizer;
typedef std::shared_ptr<H264Repacketizer> H264RepacketizerPtr;


class H264Repacketizer : public RtpRepacketizer
{
public:
    static RtpRepacketizerPtr create(std::shared_ptr<rtc::RtpPacketizationConfig> rtpConf) { return std::make_shared<H264Repacketizer>(rtpConf); }


    H264Repacketizer(std::shared_ptr<rtc::RtpPacketizationConfig> rtpConf) : RtpRepacketizer(rtpConf->ssrc, rtpConf->payloadType), rtpConf_(rtpConf)
    {
        // TODO: remove this workaround for https://github.com/paullouisageneau/libdatachannel/issues/1216
        rtpConf_->playoutDelayId = 0;
        packet_ = std::make_shared<rtc::H264RtpPacketizer>(rtc::NalUnit::Separator::LongStartSequence, rtpConf_);

    }

    std::vector<std::vector<uint8_t>> handlePacket(std::vector<uint8_t> data)
    {
        std::vector<std::vector<uint8_t>> ret;
        if (data.size() < 2 || data.at(1) == 200) {
            // ignore RTCP packets for now
            return ret;
        }

        auto src = reinterpret_cast<const std::byte*>(data.data());
        rtc::message_ptr msg = std::make_shared<rtc::Message>(src, src + data.size());

        rtc::message_vector vec;
        vec.push_back(msg);

        depacket_.incoming(vec, nullptr);

        if (vec.size() > 0) {
            rtpConf_->timestamp = vec[0]->frameInfo->timestamp;
            packet_->outgoing(vec, nullptr);

            for (auto m : vec) {
                uint8_t* src = (uint8_t*)m->data();
                ret.push_back(std::vector<uint8_t>(src, src+m->size()));
            }
        }
        return ret;
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
        return std::make_shared<H264Repacketizer>(rtpConf);
    }
};

} // namespace
