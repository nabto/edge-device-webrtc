#include "track_negotiator.hpp"
#include <rtc/rtp.hpp>

namespace nabto {

    Repacketizer::Repacketizer(MediaTrackPtr track, rtc::SSRC ssrc, int dstPayloadType):
        track_(track), ssrc_(ssrc), dstPayloadType_(dstPayloadType)
    {
    }

    void Repacketizer::handlePacket(uint8_t* buffer, size_t length)
    {
        auto rtp = reinterpret_cast<rtc::RtpHeader*>(buffer);
        rtp->setSsrc(ssrc_);
        rtp->setPayloadType(dstPayloadType_);

        track_->send(buffer, length);
    }

} // namespace
