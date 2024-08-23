#pragma once

#include "rtp_packetizer.hpp"

#include <rtc/h264rtppacketizer.hpp>
#include <rtc/rtppacketizer.hpp>
#include <rtc/rtppacketizationconfig.hpp>

#include <chrono>

namespace nabto {

class PcmuPacketizer : public RtpPacketizer
{
public:
    static RtpPacketizerPtr create(uint32_t ssrc, std::string& trackId, int pt) { return std::make_shared<PcmuPacketizer>(ssrc, trackId, pt); }

    PcmuPacketizer(uint32_t ssrc, std::string& trackId, int pt) {
        rtpConf_ = std::make_shared<rtc::RtpPacketizationConfig>(ssrc, trackId, pt, 8000);
        packetizer_ = std::make_shared<rtc::RtpPacketizer>(rtpConf_);

    }

    std::vector<std::vector<uint8_t> > packetize(std::vector<uint8_t> data);

    std::vector<std::vector<uint8_t> > incoming(const std::vector<uint8_t>& data);

private:
    std::shared_ptr<rtc::RtpPacketizationConfig> rtpConf_;
    std::shared_ptr<rtc::RtpPacketizer> packetizer_;
    std::vector<uint8_t> buffer_;

};

class PcmuPacketizerFactory : public RtpPacketizerFactory
{
public:
    static RtpPacketizerFactoryPtr create(const std::string& trackId) {
        return std::make_shared<PcmuPacketizerFactory>(trackId);
    }
    PcmuPacketizerFactory(const std::string& trackId): RtpPacketizerFactory(trackId) { }
    RtpPacketizerPtr createPacketizer(uint32_t ssrc, int pt) {
        return PcmuPacketizer::create(ssrc, trackId_, pt);
    }
};

} // namespace
