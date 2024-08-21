#include <rtc/rtc.hpp>

#include "h264_packetizer.hpp"
#include <nabto/nabto_device_webrtc.hpp>

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

std::vector<std::vector<uint8_t> > NalUnit::packetize()
{
    std::vector<std::vector<uint8_t> > ret;

    if (empty()) {
        NPLOGD << "packetizing empty packet";
        return ret;
    }

    rtc::message_vector vec;
    size_t i = 0;
    uint8_t nalHead = data_.front();

    if (data_.size() < MTU) {
        // NAL unit fits in single packet
        uint8_t* src = data_.data() + i;
        auto msg = std::make_shared<rtc::Message>((std::byte*)src, (std::byte*)(src + data_.size() - i));
        vec.push_back(msg);
    }
    else {
        // NAL unit must be split into fragments
        bool first = true;
        while (i + 1 < data_.size()) {
            std::vector<uint8_t> tmp;
            size_t len = i + 1 + MTU > data_.size() ? data_.size() - i - 1 : MTU;
            std::vector<uint8_t> fragment = { data_.begin() + i + 1, data_.begin() + i + 1 + len };

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
            auto msg = std::make_shared<rtc::Message>((std::byte*)src, (std::byte*)(src + tmp.size()));
            vec.push_back(msg);
            i += len;
        }
    }

    // Insert RTP headers
    rtp_->outgoing(vec, nullptr);

    for (size_t i = 0; i < vec.size()-1; i++) {
        ret.push_back(std::vector<uint8_t>((uint8_t*)vec[i]->data(), (uint8_t*)(vec[i]->data() + vec[i]->size())));
    }

    auto last = std::vector<uint8_t>((uint8_t*)vec.back()->data(), (uint8_t*)(vec.back()->data() + vec.back()->size()));

    if (shouldMark_) {
        last[1] = last[1] | RTP_MARKER_MASK;
    }
    ret.push_back(last);
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

            NalUnit nal(tmp, packetizer_);

            std::vector<std::vector<uint8_t>> packets;
            if (!isShort && !lastNal_.isPsOrAUD()) {
                /* Long separators are used for NAL units when:
                 *  - NAL unit type is SPS or PPS
                 *  - The NAL unit is the first in an Access Unit (AU)
                 *
                 * Since parameter sets must come before the encoded video frame, they will start a new AU unless a new AU was just started.
                 *
                 * Since we insert AUs if the byte stream does not have them, we should start a new AU on long separators unless lastNal_ is one of AUD, PPS, SPS.
                 */

                // On new AU, last NAL should be marked and packetized
                lastNal_.setMarker(true);
                packets = lastNal_.packetize();

                // We tick RTP timestamp between AUs
                updateTimestamp();

                if (!nal.isNalType(NAL_AUD)) {
                    // We start new AU, but stream is not using AUD
                    // so we add AUD manually.

                    ret.insert(ret.end(), packets.begin(), packets.end());
                    std::vector<uint8_t> accessUnit = { 0x00, 0x00, 0x00, 0x01, 0x09 };

                    accessUnit.push_back(nal.makeAudPayload());
                    lastNal_ = NalUnit(accessUnit, packetizer_);
                }
            } else {
                // Packetize last NAL since we know it does not need to be marked
                packets = lastNal_.packetize();
            }

            // Insert last NAL unit
            ret.insert(ret.end(), packets.begin(), packets.end());
            // Packetize current NAL unit
            lastNal_ = nal;

            // Remove current NAL unit from buffer.
            buffer_ = std::vector<uint8_t>(it, buffer_.end());
            isShort = nextShort;


        } else {
            return ret;
        }
    }
    return ret;
}

void H264Packetizer::updateTimestamp()
{
    std::chrono::milliseconds now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    );

    auto startTs = rtpConf_->startTimestamp;
    auto epochDiff = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_).count();
    rtpConf_->timestamp = startTs + (epochDiff * 90);


}

bool NalUnit::isPsOrAUD()
{
    return isNalType(NAL_SPS) || isNalType(NAL_PPS) || isNalType(NAL_AUD);
}

uint8_t NalUnit::makeAudPayload()
{
    if (isNalType(NAL_SPS) ||
        isNalType(NAL_PPS) ||
        isNalType(NAL_IDR)) {
        return 0x10;
    }
    else {
        return 0x30;
    }
}


} // namespace
