#include "rtp_repacketizer.hpp"
#include <rtc/rtp.hpp>

namespace nabto {

RtpRepacketizer::RtpRepacketizer(rtc::SSRC ssrc, int dstPayloadType) :
        ssrc_(ssrc), dstPayloadType_(dstPayloadType)
{ }

std::vector<std::vector<uint8_t>> RtpRepacketizer::handlePacket(std::vector<uint8_t> data)
{
    auto rtp = reinterpret_cast<rtc::RtpHeader*>(data.data());
    rtp->setSsrc(ssrc_);
    rtp->setPayloadType(dstPayloadType_);
    std::vector<std::vector<uint8_t>> ret = {data};
    return ret;
}

} // namespace
