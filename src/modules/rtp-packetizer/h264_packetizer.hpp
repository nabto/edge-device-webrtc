#pragma once

#include "rtp_packetizer.hpp"

#include <rtc/h264rtppacketizer.hpp>
#include <rtc/rtppacketizer.hpp>
#include <rtc/rtppacketizationconfig.hpp>

#include <chrono>

namespace nabto {

class H264Packetizer : public RtpPacketizer
{
public:
    static RtpPacketizerPtr create(uint32_t ssrc, std::string& trackId, int pt) { return std::make_shared<H264Packetizer>(ssrc, trackId, pt); }

    H264Packetizer(uint32_t ssrc, std::string& trackId, int pt) {
        rtpConf_ = std::make_shared<rtc::RtpPacketizationConfig>(ssrc, trackId, pt, 90000);
        // packetizer_ = std::make_shared<rtc::H264RtpPacketizer>(rtc::NalUnit::Separator::StartSequence, rtpConf_);
        packetizer_ = std::make_shared<rtc::RtpPacketizer>(rtpConf_);

        start_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );
    }

    std::vector<std::vector<uint8_t> > packetize(std::vector<uint8_t> data, bool isShort);

    std::vector<std::vector<uint8_t> > incoming(const std::vector<uint8_t>& data);

private:
    std::shared_ptr<rtc::RtpPacketizationConfig> rtpConf_;
    // std::shared_ptr<rtc::H264RtpPacketizer> packetizer_;
    std::shared_ptr<rtc::RtpPacketizer> packetizer_;
    std::chrono::milliseconds start_;
    std::vector<uint8_t> buffer_;
    std::vector<std::vector<uint8_t>> lastNal_;
    uint8_t lastNalHead_ = 0;

};

} // namespace
