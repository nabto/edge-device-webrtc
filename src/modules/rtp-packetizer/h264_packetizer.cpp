#include "h264_packetizer.hpp"
#include <nabto/nabto_device_webrtc.hpp>

#include "rtc/message.hpp"

#include <algorithm>

/*
 * This can be used to packetize a H264 byte-stream conforming to the annex B specifications in "H.264 : Advanced video coding for generic audiovisual services" https://www.itu.int/rec/T-REC-H.264-202108-I/en
 * This means NAL units must be separated by the "start code prefix". H264 feeds using the length separator (MP4) will not work.
 */

namespace nabto {

std::vector<uint8_t> shortSep = { 0x00, 0x00, 0x01 };

const int MTU = 1200;
const uint8_t NAL_IDR = 5;
const uint8_t NAL_SPS = 7;
const uint8_t NAL_PPS = 8;
const uint8_t NAL_AUD = 9;
const uint8_t NAL_FUA = 28;
const uint8_t RTP_MARKER_MASK = 0x80;

uint8_t setStart(bool isSet, uint8_t type) { return (type & 0x7F) | (isSet << 7); }
uint8_t setEnd(bool isSet, uint8_t type) { return (type & 0b1011'1111) | (isSet << 6); }
bool isNalType(uint8_t head, uint8_t type) { return (head & 0b00011111) == type; }

std::vector<std::vector<uint8_t> > H264Packetizer::packetize(std::vector<uint8_t> data)
{
    std::vector<std::vector<uint8_t> > ret;

    rtc::message_vector vec;
    size_t i = 0;
    uint8_t nalHead = data.front();

    if (data.size() < MTU) {
        // NAL unit fits in single packet
        uint8_t* src = data.data() + i;
        auto msg = std::make_shared<rtc::Message>((std::byte*)src, (std::byte*)(src + data.size() - i));
        vec.push_back(msg);
    } else {
        // NAL unit must be split into fragments
        bool first = true;
        while (i+1 < data.size()) {
            std::vector<uint8_t> tmp;
            size_t len = i + 1 + MTU > data.size() ? data.size() - i - 1 : MTU;
            std::vector<uint8_t> fragment = { data.begin() + i + 1, data.begin() + i + 1 + len };

            // FU identifier becomes the NAL header, so we must keep NRI from the original NAL unit and change the type to FU-A
            uint8_t fuIndentifier = (nalHead & 0b11100000) + NAL_FUA;
            // FU Header stores the original NAL type in the 5 lowest bits
            uint8_t fuHeader = nalHead & 0b00011111;

            // Set FU Header start/end markers
            fuHeader = setStart(first, fuHeader);
            first = false;
            fuHeader = setEnd((len != MTU), fuHeader);

            tmp.push_back(fuIndentifier);
            tmp.push_back(fuHeader);

            tmp.insert(tmp.end(), fragment.begin(), fragment.end());

            uint8_t* src = tmp.data();
            auto msg = std::make_shared<rtc::Message>((std::byte*)src, (std::byte*)(src+tmp.size()));
            vec.push_back(msg);
            i += len;
        }
    }

    // Insert RTP headers
    packetizer_->outgoing(vec, nullptr);

    for (auto m : vec) {
        ret.push_back(std::vector<uint8_t>((uint8_t*)m->data(), (uint8_t*)(m->data() + m->size())));
    }
    return ret;
}

std::vector<std::vector<uint8_t> > H264Packetizer::incoming(const std::vector<uint8_t>& data)
{
    buffer_.insert(buffer_.end(), data.begin(), data.end());
    std::vector<std::vector<uint8_t> > ret;

    bool isShort = false; // True if current NAL unit uses Short Separator
    if (buffer_.size() > 3 && buffer_.at(2) == 0x01) {
        // buffer starts with short separator: 0x00, 0x00, 0x01
        isShort = true;
    }

    while(1){
        // search for a second nal unit separator
        auto it = std::search(buffer_.begin() + 4, buffer_.end(), shortSep.begin(), shortSep.end());

        if (it != buffer_.end()) {
            // If we found a separator. (if long exists, so does the short)
            bool nextShort = true;
            if (*(it-1) == 0x00) {
                it--;
                nextShort = false; // we found a long separator
            }

            auto tmp = std::vector<uint8_t>(buffer_.begin()+(isShort ? 3 : 4), it);
            while(tmp.back() == 0x00) { tmp.pop_back(); }

            if (!isShort) {
                /* Long separators are used for NAL units when:
                 *  - NAL unit type is SPS or PPS
                 *  - The NAL unit is the first in an Access Unit (AU)
                 *
                 * Since parameter sets must come before the encoded video frame, they will start a new AU unless a new AU was just started.
                 *
                 * Since we insert AUs if the byte stream does not have them, we should start a new AU on long separators unless lastNal_ is one of AUD, PPS, SPS.
                 */

                // Start new AU on long separator
                if (!isNalType(lastNalHead_, NAL_SPS) &&
                    !isNalType(lastNalHead_, NAL_PPS) &&
                    !isNalType(lastNalHead_, NAL_AUD) ) {
                    // Unless last NAL was SPS, PPS, or AUD
                    if (lastNal_.size() > 0) {
                        // Last NAL exists
                        // If stream uses AUD, this is not an SPS NAL unit
                        // If stream does not use AUD, this is an SPS NAL unit
                        // This means we must set the marker-bit in the previous RTP header
                        auto r = lastNal_.back();
                        lastNal_.pop_back();
                        r[1] = r[1] | RTP_MARKER_MASK;
                        lastNal_.push_back(r);
                    }

                    // We tick RTP timestamp between AUs
                    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()
                    );

                    auto startTs = rtpConf_->startTimestamp;
                    auto epochDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
                    rtpConf_->timestamp = startTs + (epochDiff * 90);

                    if (!isNalType(tmp.front(), NAL_AUD)) {
                        // We start new AU, but stream is not using AUD
                        // so we add AUD manually.
                        ret.insert(ret.end(), lastNal_.begin(), lastNal_.end());
                        std::vector<uint8_t> accessUnit = { 0x00, 0x00, 0x00, 0x01, 0x09 };
                        if (isNalType(tmp.front(), NAL_SPS) ||
                            isNalType(tmp.front(), NAL_PPS) ||
                            isNalType(tmp.front(), NAL_IDR)) {
                            // If this is parameter set or IDR, we are also starting an new coded video sequence, so payload must be 0x10
                            accessUnit.push_back(0x10);
                        } else {
                            accessUnit.push_back(0x30);
                        }
                        lastNal_ = packetize(accessUnit);
                        lastNalHead_ = NAL_AUD;
                    }
                }
            }

            // Insert last NAL unit since we know if needs to be marked
            ret.insert(ret.end(), lastNal_.begin(), lastNal_.end());
            lastNalHead_ = tmp.front();
            // Packetize current NAL unit
            lastNal_ = packetize(tmp);

            // Remove current NAL unit from buffer.
            buffer_ = std::vector<uint8_t>(it, buffer_.end());
            isShort = nextShort;


        } else {
            return ret;
        }
    }
    return ret;
}

} // namespace
