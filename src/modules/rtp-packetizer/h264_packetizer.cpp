#include "h264_packetizer.hpp"

#include "rtc/message.hpp"

#include <algorithm>

namespace nabto {

std::vector<uint8_t> accessDelimiter = {0x00, 0x00, 0x00, 0x01, 0x09};

std::vector<std::vector<uint8_t> > H264Packetizer::incoming(const std::vector<uint8_t>& data)
{
    buffer_.insert(buffer_.end(), data.begin(), data.end());
    auto it = std::search(buffer_.begin()+4, buffer_.end(), accessDelimiter.begin(), accessDelimiter.end());

    if (it != buffer_.end()) {
        auto tmp = std::vector<uint8_t>(buffer_.begin(), it);
        auto accessUnit = std::make_shared<rtc::Message>((std::byte*)tmp.data(), (std::byte*)(tmp.data() + tmp.size()));
        rtc::message_vector vec;
        vec.push_back(accessUnit);

        std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );

        auto startTs = rtpConf_->startTimestamp;
        auto epochDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_).count();
        rtpConf_->timestamp = startTs + (epochDiff * 90);


        packetizer_->outgoing(vec, nullptr);

        std::vector<std::vector<uint8_t> > ret;

        for (auto m : vec) {
            ret.push_back(std::vector<uint8_t>((uint8_t*)m->data(), (uint8_t*)(m->data()+m->size())));
        }

        buffer_ = std::vector<uint8_t>(it, buffer_.end());

        return ret;

    }

    return std::vector<std::vector<uint8_t> >();
}

} // namespace
