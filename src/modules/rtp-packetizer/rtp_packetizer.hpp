#pragma once

#include <memory>
#include <vector>

namespace nabto {

class RtpPacketizer;
typedef std::shared_ptr<RtpPacketizer> RtpPacketizerPtr;

class RtpPacketizer
{
public:
    virtual std::vector<std::vector<uint8_t> > incoming(const std::vector<uint8_t>& data) = 0;
};

} // namespace
