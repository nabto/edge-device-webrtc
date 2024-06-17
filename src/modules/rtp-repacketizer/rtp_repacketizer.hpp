#pragma once

#include <nabto/nabto_device_webrtc.hpp>

#include <memory>
#include <vector>
#include <string>

namespace nabto {

class RtpRepacketizer;
typedef std::shared_ptr<RtpRepacketizer> RtpRepacketizerPtr;

class RtpRepacketizerFactory;
typedef std::shared_ptr<RtpRepacketizerFactory> RtpRepacketizerFactoryPtr;


class RtpRepacketizer {
public:
    RtpRepacketizer(uint32_t ssrc, int dstPayloadType);
    virtual std::vector<std::vector<uint8_t>> handlePacket(std::vector<uint8_t> data);

protected:
    uint32_t ssrc_;
    int dstPayloadType_;
};


class RtpRepacketizerFactory
{
public:
    static RtpRepacketizerFactoryPtr create() {
        return std::make_shared<RtpRepacketizerFactory>();
    }

    RtpRepacketizerFactory() {}
    virtual RtpRepacketizerPtr createPacketizer(MediaTrackPtr track, uint32_t ssrc, int dstPayloadType) {
        return std::make_shared<RtpRepacketizer>(ssrc, dstPayloadType);
    };
};

} // namespace
