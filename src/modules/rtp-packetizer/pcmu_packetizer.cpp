#include "pcmu_packetizer.hpp"
#include <nabto/nabto_device_webrtc.hpp>

#include "rtc/message.hpp"
#include <rtc/rtppacketizer.hpp>

#include <algorithm>

namespace nabto {


std::vector<std::vector<uint8_t> > PcmuPacketizer::incoming(const std::vector<uint8_t>& data)
{
    buffer_.insert(buffer_.end(), data.begin(), data.end());
    std::vector<std::vector<uint8_t> > ret;

    // PCMU timestamp should match sample count in packets, so we packetize in fixed 1024 byte payloads
    while (buffer_.size() >= 1024) {
        auto msg = std::make_shared<rtc::Message>((std::byte*)buffer_.data(), (std::byte*)(buffer_.data() + 1024));
        rtc::message_vector vec;
        vec.push_back(msg);
        buffer_.erase(buffer_.begin(), buffer_.begin()+1024);
        rtpConf_->timestamp += 1024;
        packetizer_->outgoing(vec, nullptr);

        for (size_t i = 0; i < vec.size(); i++) {
            ret.push_back(std::vector<uint8_t>((uint8_t*)vec[i]->data(), (uint8_t*)(vec[i]->data() + vec[i]->size())));
        }
    }
    return ret;
}

} // namespace
