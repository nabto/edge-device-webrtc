#pragma once

#include "rtp_packetizer.hpp"

#include <rtc/h264rtppacketizer.hpp>
#include <rtc/rtppacketizer.hpp>
#include <rtc/rtppacketizationconfig.hpp>

#include <chrono>

namespace nabto {

class NalUnit {
public:
    NalUnit() {}
    NalUnit(std::vector<uint8_t>& data, std::shared_ptr<rtc::RtpPacketizer> rtp) : data_(data), rtp_(rtp)
    {
        header_ = data.front();
    }

    std::vector<std::vector<uint8_t> > packetize();

    bool empty() {return data_.empty();}

    void setMarker(bool mark) { shouldMark_ = mark; }

    bool isNalType(uint8_t type) { return (header_ & 0b00011111) == type; }

    bool isPsOrAUD();

    uint8_t makeAudPayload();

private:
    uint8_t setStart(bool isSet, uint8_t type) { return (type & 0x7F) | (isSet << 7); }
    uint8_t setEnd(bool isSet, uint8_t type) { return (type & 0b1011'1111) | (isSet << 6); }


    std::vector<uint8_t> data_;
    std::shared_ptr<rtc::RtpPacketizer> rtp_ = nullptr;
    uint8_t header_ = 0;
    bool shouldMark_ = false;

};




class H264Packetizer : public RtpPacketizer
{
public:
    static RtpPacketizerPtr create(uint32_t ssrc, std::string& trackId, int pt) { return std::make_shared<H264Packetizer>(ssrc, trackId, pt); }

    H264Packetizer(uint32_t ssrc, std::string& trackId, int pt) {
        rtpConf_ = std::make_shared<rtc::RtpPacketizationConfig>(ssrc, trackId, pt, 90000);
        // TODO: remove this workaround for https://github.com/paullouisageneau/libdatachannel/issues/1216
        rtpConf_->playoutDelayId = 0;
        start_ = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );
        packetizer_ = std::make_shared<rtc::RtpPacketizer>(rtpConf_);

    }

    std::vector<std::vector<uint8_t> > packetize(std::vector<uint8_t> data);

    std::vector<std::vector<uint8_t> > incoming(const std::vector<uint8_t>& data);

private:

    void updateTimestamp();

    std::shared_ptr<rtc::RtpPacketizationConfig> rtpConf_;
    std::shared_ptr<rtc::RtpPacketizer> packetizer_;
    std::chrono::milliseconds start_;
    std::vector<uint8_t> buffer_;
    NalUnit lastNal_;

};

class H264PacketizerFactory : public RtpPacketizerFactory
{
public:
    static RtpPacketizerFactoryPtr create(const std::string& trackId) {
        return std::make_shared<H264PacketizerFactory>(trackId);
    }
    H264PacketizerFactory(const std::string& trackId): RtpPacketizerFactory(trackId) { }
    RtpPacketizerPtr createPacketizer(uint32_t ssrc, int pt) {
        return H264Packetizer::create(ssrc, trackId_, pt);
    }
};

} // namespace
