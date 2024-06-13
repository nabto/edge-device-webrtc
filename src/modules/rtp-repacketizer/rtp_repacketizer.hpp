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
    RtpRepacketizer(MediaTrackPtr track, uint32_t ssrc, int dstPayloadType);
    virtual void handlePacket(uint8_t* buffer, size_t length);

protected:
    MediaTrackPtr track_;
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
        return std::make_shared<RtpRepacketizer>(track, ssrc, dstPayloadType);
    };
};

} // namespace
