#include "h264_packetizer.hpp"
#include <nabto/nabto_device_webrtc.hpp>

#include "rtc/message.hpp"

#include <algorithm>

namespace nabto {

std::vector<uint8_t> aud = { 0x00, 0x00, 0x00, 0x01, 0x09 };

std::vector<uint8_t> longSep = { 0x00, 0x00, 0x00, 0x01 };
std::vector<uint8_t> shortSep = { 0x00, 0x00, 0x01 };

const int MTU = 1200;
const int NAL_FUA = 28;

uint8_t setStart(bool isSet, uint8_t type) { return (type & 0x7F) | (isSet << 7); }
uint8_t setEnd(bool isSet, uint8_t type) { return (type & 0b1011'1111) | (isSet << 6); }
uint8_t unitType(uint8_t head) { return (head & 0b0111'1110) >> 1; }

std::vector<std::vector<uint8_t> > H264Packetizer::packetize(std::vector<uint8_t> data, bool isShort)
{
    std::vector<std::vector<uint8_t> > ret;

    if (data[2] == 1) {
        isShort = true;
    } else {
        isShort = false;
    }

    rtc::message_vector vec;
    size_t i = isShort ? 3 : 4;
    uint8_t nalHead = data[i];

    if (data.size() < MTU) {
        NPLOGE << "Packetize with data size < MTU: " << data.size();
        uint8_t* src = data.data() + i;
        auto msg = std::make_shared<rtc::Message>((std::byte*)src, (std::byte*)(src + data.size() - i));
        vec.push_back(msg);
    } else {
        NPLOGE << "Packetize with data size > MTU: " << data.size() << " data[0-5]: " << std::hex << (int)data[0] << ", " << (int)data[1] << ", " << (int)data[2] << ", " << (int)data[3] << ", " << (int)data[4] << ", " << (int)data[5] << std::dec;
        bool first = true;
        while (i < data.size()) {
            std::vector<uint8_t> tmp;
            size_t len = i + MTU > data.size() ? data.size() - i : MTU;
            std::vector<uint8_t> fragment = {data.begin()+ i+1, data.begin()+i+1+len};

            uint8_t fuIndic = (nalHead & 0b11100000) + NAL_FUA;
            uint8_t fuHeader = nalHead & 0b00011111;

            if (first) {
                fuHeader = setStart(true, fuHeader);
                first = false;
            } else {
                fuHeader = setStart(false, fuHeader);
            }

            if (len != MTU) {
                fuHeader = setEnd(true, fuHeader);
            }
            else {
                fuHeader = setEnd(false, fuHeader);
            }
            NPLOGE << "Building fragment with fuIdic: " << std::hex << (int)fuIndic << " fuHead: " << (int)fuHeader << " nalHead: " << (int)nalHead << std::dec << " len: " << (int)len;

            tmp.push_back(fuIndic);
            tmp.push_back(fuHeader);

            tmp.insert(tmp.end(), fragment.begin(), fragment.end());

            uint8_t* src = tmp.data();
            auto msg = std::make_shared<rtc::Message>((std::byte*)src, (std::byte*)(src+tmp.size()));
            vec.push_back(msg);
            i += len;
        }
    }

    NPLOGE << "RTP packetizing " <<  vec.size() << " packets";
    packetizer_->outgoing(vec, nullptr);

    NPLOGE << "RTP packetized " << vec.size() << " packets";
    for (auto m : vec) {
        ret.push_back(std::vector<uint8_t>((uint8_t*)m->data(), (uint8_t*)(m->data() + m->size())));
    }
    return ret;
}

std::vector<std::vector<uint8_t> > H264Packetizer::incoming(const std::vector<uint8_t>& data)
{
    buffer_.insert(buffer_.end(), data.begin(), data.end());
    std::vector<std::vector<uint8_t> > ret;

    // std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
    //     std::chrono::system_clock::now().time_since_epoch()
    // );

    // auto startTs = rtpConf_->startTimestamp;
    // auto epochDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_).count();
    // rtpConf_->timestamp = startTs + (epochDiff * 90);

    if (std::search(buffer_.begin() + 4, buffer_.end(), aud.begin(), aud.end()) == buffer_.end())
    {
        return ret;
    }


    while(1){
        auto it = std::search(buffer_.begin() + 4, buffer_.end(), longSep.begin(), longSep.end());
        auto it2 = std::search(buffer_.begin() + 4, buffer_.end(), shortSep.begin(), shortSep.end());
        bool isShort = false;

        if (it != buffer_.end()) {
            NPLOGE << "Found LongSep at: ";
        }

        if (it2 != buffer_.end()) {
            NPLOGE << "Found ShortSep at: ";
        }

        if (it2 != buffer_.end()) {
            if (it2 < it) {
                NPLOGE << "Using ShortSep";
                it = it2;
                isShort = true;
            }

            auto tmp = std::vector<uint8_t>(buffer_.begin(), it);

            if (buffer_.at(4) == 9) {
                if (lastNal_.size() > 0) {
                    NPLOGE << "Marking last NAL";
                    auto r = lastNal_.back();
                    lastNal_.pop_back();
                    r[1] = r[1] | 0x80;
                    lastNal_.push_back(r);
                }
                std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                );

                auto startTs = rtpConf_->startTimestamp;
                auto epochDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_).count();
                rtpConf_->timestamp = startTs + (epochDiff * 90);

            }

            ret.insert(ret.end(), lastNal_.begin(), lastNal_.end());
            lastNal_ = packetize(tmp, isShort);

            // auto ps = packetize(tmp, isShort);
            // ret.insert(ret.end(), ps.begin(), ps.end());
            buffer_ = std::vector<uint8_t>(it, buffer_.end());


            // auto accessUnit = std::make_shared<rtc::Message>((std::byte*)tmp.data(), (std::byte*)(tmp.data() + tmp.size()));
            // rtc::message_vector vec;
            // vec.push_back(accessUnit);

            // std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
            //     std::chrono::system_clock::now().time_since_epoch()
            // );

            // auto startTs = rtpConf_->startTimestamp;
            // auto epochDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_).count();
            // rtpConf_->timestamp = startTs + (epochDiff * 90);


            // packetizer_->outgoing(vec, nullptr);


            // for (auto m : vec) {
            //     ret.push_back(std::vector<uint8_t>((uint8_t*)m->data(), (uint8_t*)(m->data()+m->size())));
            // }

            // buffer_ = std::vector<uint8_t>(it, buffer_.end());

        } else {
            return ret;
        }
    }
    return ret;
}

} // namespace
