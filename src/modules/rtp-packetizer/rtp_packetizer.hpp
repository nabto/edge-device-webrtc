#pragma once

#include <memory>
#include <vector>
#include <string>

namespace nabto {

class RtpPacketizer;
typedef std::shared_ptr<RtpPacketizer> RtpPacketizerPtr;

class RtpPacketizerFactory;
typedef std::shared_ptr<RtpPacketizerFactory> RtpPacketizerFactoryPtr;

class RtpPacketizer
{
public:
    virtual std::vector<std::vector<uint8_t> > incoming(const std::vector<uint8_t>& data) = 0;
};

class RtpPacketizerFactory
{
public:
    RtpPacketizerFactory(std::string& trackId) : trackId_(trackId) {}
    virtual RtpPacketizerPtr createPacketizer(uint32_t ssrc, int pt) = 0;
protected:
    std::string trackId_;
};

} // namespace
