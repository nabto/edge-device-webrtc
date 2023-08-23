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

void RtpClient::addTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc)
{
    try {
        auto media = track->description();
        {
            std::lock_guard<std::mutex> lock(mutex_);

            const rtc::SSRC ssrc = matcher_->ssrc();
            int ptWebrtc = matcher_->match(&media);
            int ptRtp = matcher_->payloadType();
            media.addSSRC(ssrc, std::string(trackId_));
            auto track = pc->addTrack(media);
            // TODO: random ssrc
            RtpTrack videoTrack = {
                pc,
                track,
                ssrc,
                ptRtp,
                ptWebrtc
            };
            std::cout << "adding track with pt " << videoTrack.srcPayloadType << "->" << videoTrack.dstPayloadType << std::endl;
            videoTracks_.push_back(videoTrack);
            if (stopped_) {
                start();
            }
        }
        // TODO: add receiving data as well
    }
    catch (std::exception ex) {
        std::cout << "GOT EXCEPTION!!! " << ex.what() << std::endl;
    }

}

std::shared_ptr<rtc::Track> RtpClient::createTrack(std::shared_ptr<rtc::PeerConnection> pc)

{
    std::lock_guard<std::mutex> lock(mutex_);

    // TODO: random ssrc
    const rtc::SSRC ssrc = matcher_->ssrc();

    auto media = matcher_->createMedia();
    media.addSSRC(ssrc, trackId_);
    auto track = pc->addTrack(media);

    RtpTrack videoTrack = {
        pc,
        track,
        ssrc,
        matcher_->payloadType(),
        matcher_->payloadType()
    };
    std::cout << "adding track with pt " << videoTrack.srcPayloadType << "->" << videoTrack.dstPayloadType << std::endl;
    videoTracks_.push_back(videoTrack);
    if (stopped_) {
        start();
    }
    if (matcher_->direction() != RtpCodec::SEND_ONLY) {
        // We are also gonna receive data
        auto self = shared_from_this();
        int count = 0;
        track->onMessage([self, videoTrack, &count](rtc::message_variant data) {
            auto msg = rtc::make_message(data);
            if (msg->type == rtc::Message::Binary) {
                rtc::byte* data = msg->data();
                auto rtp = reinterpret_cast<rtc::RtpHeader *>(data);
                uint8_t pt = rtp->payloadType();
                if (pt != videoTrack.dstPayloadType) {
                    return;
                }
                count++;
                if (count % 100 == 0) {
                    std::cout << ":";
                }
                if (count % 1600 == 0) {
                    std::cout << std::endl;
                    count = 0;
                }

                rtp->setSsrc(videoTrack.srcPayloadType);

                struct sockaddr_in addr = {};
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = inet_addr(self->remoteHost_.c_str());
                addr.sin_port = htons(self->remotePort_);

                auto ret = sendto(self->videoRtpSock_, data, msg->size(), 0, (struct sockaddr*)&addr, sizeof(addr));
                // ssize_t ret = write(self->rtcpSock_, data, msg->size());
                // std::cout << "Wrote " << ret << " bytes to socket port: " << self->remotePort_ << std::endl;


            }
        });
    }

    return track;
}

void RtpClient::removeConnection(std::shared_ptr<rtc::PeerConnection> pc)
{
    std::cout << "Removing PeerConnection from RTP" << std::endl;
    size_t videoTracksSize = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (std::vector<RtpTrack>::iterator it = videoTracks_.begin(); it != videoTracks_.end(); it++) {
            if (pcPtrComp(it->pc, pc)) {
                std::cout << "Found PeerConnection" << std::endl;
                videoTracks_.erase(it);
                break;
            }
        }
        videoTracksSize = videoTracks_.size();
    }
    if (videoTracksSize == 0) {
        std::cout << "PeerConnection was last one. Stopping" << std::endl;
        stop();
    } else {
        std::cout << "Still " << videoTracksSize << " PeerConnections. Not stopping" << std::endl;
    }
}

std::string RtpClient::getTrackId()
{
    return trackId_;
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
    {
        std::lock_guard<std::mutex> lock(mutex_);

        stopped_ = true;
        if (videoRtpSock_ != 0) {
            close(videoRtpSock_);
        }
    }
    videoThread_.join();
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
            for (auto t : self->videoTracks_) {
                if (!t.track || !t.track->isOpen()) {
                    continue;
                }
                rtp->setSsrc(t.ssrc);
                rtp->setPayloadType(t.dstPayloadType);
                // std::cout << "Sending RTP: " << std::endl;
                // for (int i = 0; i < len; i++) {
                //     std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)i;
                // }
                // std::cout << std::dec << std::endl;

                t.track->send(reinterpret_cast<const rtc::byte*>(buffer), len);
            }
        }

    }

}


int H264CodecMatcher::match(rtc::Description::Media* media)
{
    rtc::Description::Media::RtpMap* rtp = NULL;
    for (auto pt : media->payloadTypes()) {
        rtc::Description::Media::RtpMap* r = NULL;
        try {
            r = media->rtpMap(pt);
        }
        catch (std::exception& ex) {
            // std::cout << "Bad rtpMap for pt: " << pt << std::endl;
            continue;
        }
        std::string profLvlId = "42e01f";
        std::string lvlAsymAllowed = "1";
        std::string pktMode = "1";
        if (r != NULL && r->fmtps.size() > 0 &&
            r->fmtps[0].find("profile-level-id=" + profLvlId) != std::string::npos &&
            r->fmtps[0].find("level-asymmetry-allowed=" + lvlAsymAllowed) != std::string::npos &&
            r->fmtps[0].find("packetization-mode=" + pktMode) != std::string::npos
            ) {
            std::cout << "FOUND RTP codec match!!! " << pt << std::endl;
            rtp = r;
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
            // std::cout << "no match, removing" << std::endl;
            media->removeRtpMap(pt);
        }
    }
    return rtp == NULL ? 0 : rtp->payloadType;
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

int OpusCodecMatcher::match(rtc::Description::Media* media)
{
    rtc::Description::Media::RtpMap* rtp = NULL;
    for (auto pt : media->payloadTypes()) {
        rtc::Description::Media::RtpMap* r = NULL;
        try {
            r = media->rtpMap(pt);
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
            media->removeRtpMap(pt);
        }
    }
    return rtp == NULL ? 0 : rtp->payloadType;
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

int PcmuCodecMatcher::match(rtc::Description::Media* media)
{
    rtc::Description::Media::RtpMap* rtp = NULL;
    for (auto pt : media->payloadTypes()) {
        rtc::Description::Media::RtpMap* r = NULL;
        try {
            r = media->rtpMap(pt);
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
            media->removeRtpMap(pt);
        }
    }
    return rtp == NULL ? 0 : rtp->payloadType;
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
