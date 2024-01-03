#include "rtp_client.hpp"

#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <iomanip> // For std::setfill and std::setw

const int RTP_BUFFER_SIZE = 2048;


namespace nabto {


RtpClientPtr RtpClient::create(std::string trackId)
{
    return std::make_shared<RtpClient>(trackId);
}

RtpClient::RtpClient(std::string& trackId)
    : trackId_(trackId)
{

}

RtpClient::~RtpClient()
{
    std::cout << "RtpClient destructor" << std::endl;
}


std::string RtpClient::getTrackId()
{
    return trackId_;
}

bool RtpClient::isTrack(std::string& trackId)
{
    if (trackId == trackId_) {
        return true;
    }
    return false;
}

bool RtpClient::matchMedia(MediaTrackPtr media)
{
    int pt = matcher_->match(media);
    if (pt == 0) {
        std::cout << "    CODEC MATCHING FAILED!!! " << std::endl;
        return false;
    }
    return true;
}


void RtpClient::addConnection(NabtoDeviceConnectionRef ref, MediaTrackPtr media)
{

    auto sdp = media->getSdp();
    // TODO: remove when updating libdatachannel after https://github.com/paullouisageneau/libdatachannel/issues/1074
    if (sdp[0] == 'm' && sdp[1] == '=') {
        sdp = sdp.substr(2);
    }
    rtc::Description::Media desc(sdp);
    auto pts = desc.payloadTypes();

    // exactly 1 payload type should exist, else something has failed previously, so we just pick the first one blindly.
    int pt = pts.empty() ? 0 : pts[0];

    const rtc::SSRC ssrc = matcher_->ssrc();
    RtpTrack track = {
        media,
        ssrc,
        matcher_->payloadType(),
        pt
    };
    addConnection(ref, track);
}



void RtpClient::addConnection(NabtoDeviceConnectionRef ref, RtpTrack track)
{
    std::lock_guard<std::mutex> lock(mutex_);
    mediaTracks_[ref] = track;
    std::cout << "Adding RTP connection pt " << track.srcPayloadType << "->" << track.dstPayloadType << std::endl;
    if (stopped_) {
        start();
    }

    if (matcher_->direction() != RtpCodec::SEND_ONLY) {
        // We are also gonna receive data
        std::cout << "    adding Track receiver" << std::endl;
       auto self = shared_from_this();
       track.track->setReceiveCallback([self, track](uint8_t* buffer, size_t length) {
            auto rtp = reinterpret_cast<rtc::RtpHeader*>(buffer);

            uint8_t pt = rtp->payloadType();
            if (pt != track.dstPayloadType) {
                // std::cout << "INVALID PT: " << (int)pt << " != " << track.dstPayloadType << std::endl;
                return;
            }

            rtp->setPayloadType(track.srcPayloadType);

            struct sockaddr_in addr = {};
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr(self->remoteHost_.c_str());
            addr.sin_port = htons(self->remotePort_);

            auto ret = sendto(self->videoRtpSock_, buffer, length, 0, (struct sockaddr*)&addr, sizeof(addr));
        });
    }
}

void RtpClient::removeConnection(NabtoDeviceConnectionRef ref)
{
    std::cout << "Removing Nabto Connection from RTP" << std::endl;
    size_t mediaTracksSize = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        try {
            mediaTracks_.erase(ref);
        }
        catch (std::out_of_range& ex) {
            std::cout << "Tried to remove non-existing connection" << std::endl;
        }
        mediaTracksSize = mediaTracks_.size();
    }
    if (mediaTracksSize == 0) {
        std::cout << "Connection was last one. Stopping" << std::endl;
        stop();
    }
    else {
        std::cout << "Still " << mediaTracksSize << " Connections. Not stopping" << std::endl;
    }

}

void RtpClient::start()
{
    std::cout << "Starting RTP Client listen on port " << videoPort_ << std::endl;
    stopped_ = false;
    videoRtpSock_ = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    addr.sin_port = htons(videoPort_);
    if (bind(videoRtpSock_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::string err = "Failed to bind UDP socket on 0.0.0.0:";
        err += std::to_string(videoPort_);
        std::cout << err << std::endl;
        throw std::runtime_error(err);
    }

    int rcvBufSize = 212992;
    setsockopt(videoRtpSock_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvBufSize),
        sizeof(rcvBufSize));
    videoThread_ = std::thread(rtpVideoRunner, this);
}

void RtpClient::stop()
{
    std::cout << "RtpClient stopped" << std::endl;
    bool stopped = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) {
            stopped = stopped_;
        } else {
            stopped_ = true;
            if (videoRtpSock_ != 0) {
                shutdown(videoRtpSock_, SHUT_RDWR);
                close(videoRtpSock_);
            }
        }
    }
    if (!stopped && videoThread_.joinable()) {
        videoThread_.join();
    }
    std::cout << "RtpClient thread joined" << std::endl;
}

void RtpClient::rtpVideoRunner(RtpClient* self)
{
    char buffer[RTP_BUFFER_SIZE];
    int len;
    int count = 0;
    struct sockaddr_in srcAddr;
    socklen_t srcAddrLen = sizeof(srcAddr);
    while (true) {
        {
            std::lock_guard<std::mutex> lock(self->mutex_);
            if(self->stopped_) {
                break;
            }
        }

        len = recvfrom(self->videoRtpSock_, buffer, RTP_BUFFER_SIZE, 0, (struct sockaddr*)&srcAddr, &srcAddrLen);

        if (len < 0) {
            break;
        }

        count++;
        if (count % 100 == 0) {
            std::cout << ".";
        }
        if (count % 1600 == 0) {
            std::cout << std::endl;
            count = 0;
        }
        if (len < sizeof(rtc::RtpHeader)) {
            continue;
        }
//        self->remotePort_ = ntohs(srcAddr.sin_port);

        auto rtp = reinterpret_cast<rtc::RtpHeader*>(buffer);

        {
            std::lock_guard<std::mutex> lock(self->mutex_);
            for (const auto& [key, value] : self->mediaTracks_) {
                rtp->setSsrc(value.ssrc);
                rtp->setPayloadType(value.dstPayloadType);
                try {
                    value.track->send((uint8_t*)buffer, len);
                } catch (std::runtime_error& ex) {
                    std::cout << "Failed to send on track: " << ex.what() << std::endl;
                    value.track->close();
                }
            }
        }
    }
}


int H264CodecMatcher::match(MediaTrackPtr track)
{
    auto sdp = track->getSdp();
    std::cout << "    Got offer SDP: " << sdp << std::endl;
    // TODO: remove when updating libdatachannel after https://github.com/paullouisageneau/libdatachannel/issues/1074
    if (sdp[0] == 'm' && sdp[1] == '=') {
        sdp = sdp.substr(2);
    }
    rtc::Description::Media media(sdp);

    rtc::Description::Media::RtpMap* rtp = NULL;
    bool found = false;
    for (auto pt : media.payloadTypes()) {
        rtc::Description::Media::RtpMap* r = NULL;
        try {
            r = media.rtpMap(pt);
        }
        catch (std::exception& ex) {
            std::cout << "Bad rtpMap for pt: " << pt << std::endl;
            continue;
        }
        if (r->format == "H264" && r->clockRate == 90000) {
            std::string profLvlId = "42e01f";
            std::string lvlAsymAllowed = "1";
            std::string pktMode = "1";

            if (found && r->fmtps.size() > 0 &&
                r->fmtps[0].find("profile-level-id=" + profLvlId) != std::string::npos &&
                r->fmtps[0].find("level-asymmetry-allowed=" + lvlAsymAllowed) != std::string::npos &&
                r->fmtps[0].find("packetization-mode=" + pktMode) != std::string::npos
            ) {
                // Found better match use this
                media.removeRtpMap(rtp->payloadType);
                rtp = r;
                std::cout << "FOUND RTP BETTER codec match!!! " << pt << std::endl;
            } else if (found) {
                std::cout << "h264 pt: " << pt << " no match, removing" << std::endl;
               media.removeRtpMap(pt);
                continue;
            } else {
                std::cout << "FOUND RTP codec match!!! " << pt << std::endl;
            }
            found = true; // found a match, just remove any remaining rtpMaps
            rtp = r;
            std::cout << "Format: " << rtp->format << " clockRate: " << rtp->clockRate << " encParams: " << rtp->encParams << std::endl;
            std::cout << "rtcp fbs:" << std::endl;
            for (auto s : rtp->rtcpFbs) {
                std::cout << "   " << s << std::endl;
            }
            rtp->removeFeedback("nack");
            rtp->removeFeedback("goog-remb");
            rtp->removeFeedback("transport-cc");
            rtp->removeFeedback("ccm fir");
        }
        else {
            std::cout << "pt: " << pt << " no match, removing" << std::endl;
            media.removeRtpMap(pt);
        }
    }
    if (rtp == NULL) {
        return 0;
    }
    auto trackId = track->getTrackId();
    media.addSSRC(ssrc(), trackId);
    auto newSdp = media.generateSdp();
    std::cout << "    Setting new SDP: " << newSdp << std::endl;
    track->setSdp(newSdp);
    return rtp->payloadType;
}

rtc::Description::Media H264CodecMatcher::createMedia()
{
    rtc::Description::Video media("video", rtc::Description::Direction::SendOnly);
    media.addH264Codec(96);

    auto r = media.rtpMap(96);
    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
}

int OpusCodecMatcher::match(MediaTrackPtr track)
{
    auto sdp = track->getSdp();
    std::cout << "    Got offer SDP: " << sdp << std::endl;
    // TODO: remove when updating libdatachannel after https://github.com/paullouisageneau/libdatachannel/issues/1074
    if (sdp[0] == 'm' && sdp[1] == '=') {
        sdp = sdp.substr(2);
    }
    rtc::Description::Media media(sdp);

    rtc::Description::Media::RtpMap* rtp = NULL;
    for (auto pt : media.payloadTypes()) {
        rtc::Description::Media::RtpMap* r = NULL;
        try {
            r = media.rtpMap(pt);
        }
        catch (std::exception& ex) {
            std::cout << "Bad rtpMap for pt: " << pt << std::endl;
            continue;
        }
        if (r != NULL && r->format == "opus" && r->clockRate == 48000) {
            std::cout << "Found RTP codec for audio! pt: " << r->payloadType << std::endl;
            rtp = r;
            rtp->removeFeedback("nack");
            rtp->removeFeedback("goog-remb");
            rtp->removeFeedback("transport-cc");
            rtp->removeFeedback("ccm fir");
        }
        else {
            // std::cout << "no match, removing" << std::endl;
            media.removeRtpMap(pt);
        }
    }
    if (rtp == NULL) {
        return 0;
    }
    auto trackId = track->getTrackId();
    media.addSSRC(ssrc(), trackId);
    auto newSdp = media.generateSdp();
    std::cout << "    Setting new SDP: " << newSdp << std::endl;
    track->setSdp(newSdp);
    return rtp->payloadType;
}

rtc::Description::Media OpusCodecMatcher::createMedia()
{
    rtc::Description::Audio media("audio", rtc::Description::Direction::SendRecv);
    media.addOpusCodec(111);
    auto r = media.rtpMap(111);
    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
}

int PcmuCodecMatcher::match(MediaTrackPtr track)
{
    auto sdp = track->getSdp();
    std::cout << "    Got offer SDP: " << sdp << std::endl;
    // TODO: remove when updating libdatachannel after https://github.com/paullouisageneau/libdatachannel/issues/1074
    if (sdp[0] == 'm' && sdp[1] == '=') {
        sdp = sdp.substr(2);
    }
    rtc::Description::Media media(sdp);

    rtc::Description::Media::RtpMap* rtp = NULL;
    for (auto pt : media.payloadTypes()) {
        rtc::Description::Media::RtpMap* r = NULL;
        try {
            r = media.rtpMap(pt);
        }
        catch (std::exception& ex) {
            std::cout << "Bad rtpMap for pt: " << pt << std::endl;
            continue;
        }
        if (r != NULL && r->format == "pcmu" && r->clockRate == 8000) {
            std::cout << "Found RTP codec for audio! pt: " << r->payloadType << std::endl;
            rtp = r;
            rtp->removeFeedback("nack");
            rtp->removeFeedback("goog-remb");
            rtp->removeFeedback("transport-cc");
            rtp->removeFeedback("ccm fir");
        }
        else {
            // std::cout << "no match, removing" << std::endl;
            media.removeRtpMap(pt);
        }
    }
    if (rtp == NULL) {
        return 0;
    }
    auto trackId = track->getTrackId();
    media.addSSRC(ssrc(), trackId);
    auto newSdp = media.generateSdp();
    std::cout << "    Setting new SDP: " << newSdp << std::endl;
    track->setSdp(newSdp);
    return rtp->payloadType;
}

rtc::Description::Media PcmuCodecMatcher::createMedia()
{
    rtc::Description::Audio media("audio", rtc::Description::Direction::SendRecv);
    media.addPCMUCodec(0);
    auto r = media.rtpMap(0);
    r->removeFeedback("nack");
    r->removeFeedback("goog-remb");
    return media;
}

} // namespace
