#include "rtsp_stream.hpp"

#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <iomanip>

const int RTP_BUFFER_SIZE = 2048;


namespace nabto {


RtspStreamPtr RtspStream::create(std::string trackId)
{
    return std::make_shared<RtspStream>(trackId);
}

RtspStream::RtspStream(std::string& trackId)
    : trackId_(trackId)
{

}

RtspStream::~RtspStream()
{
    std::cout << "RtspStream destructor" << std::endl;
}

void RtspStream::addTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc)
{
    try {
        auto media = track->description();
        rtc::Description::Media::RtpMap* rtp = NULL;
        for (auto pt : media.payloadTypes()) {
            rtc::Description::Media::RtpMap* r = NULL;
            try {
                r = media.rtpMap(pt);
            }
            catch (std::exception& ex) {
                // std::cout << "Bad rtpMap for pt: " << pt << std::endl;
                continue;
            }
            // TODO: make codec configureable and generalize this matching
            std::string profLvlId = "42e01f";
            // std::string lvlAsymAllowed = "1";
            // std::string pktMode = "1";
            // std::string profLvlId = "4d001f";
            std::string lvlAsymAllowed = "1";
            std::string pktMode = "1";
            if (r != NULL && r->fmtps.size() > 0 &&
                r->fmtps[0].find("profile-level-id=" + profLvlId) != std::string::npos &&
                r->fmtps[0].find("level-asymmetry-allowed=" + lvlAsymAllowed) != std::string::npos &&
                r->fmtps[0].find("packetization-mode=" + pktMode) != std::string::npos
                ) {
                std::cout << "FOUND RTP codec match!!! " << pt << std::endl;
                rtp = r;
            }
            else {
                // std::cout << "no match, removing" << std::endl;
                media.removeRtpMap(pt);
            }
        }
        // TODO: handle no match found error
        media.addSSRC(42, trackId_ + "-video");
        auto track_ = pc->addTrack(media);
        int pt = rtp == NULL ? 0 : rtp->payloadType;
        // TODO: random ssrc
        RtpTrack videoTrack = {
            pc,
            track_,
            42,
            96,
            pt
        };
        std::cout << "adding track with pt 96->" << pt << std::endl;
        // TODO: thread safety
        videoTracks_.push_back(videoTrack);
        if (stopped_) {
            start();
            auto self = shared_from_this();
            track_->onMessage([self](rtc::message_variant data) {
                // TODO: implement RTCP forwarding
                auto msg = rtc::make_message(data);
                if (msg->type == rtc::Message::Control) {
                    std::cout << "GOT CONTROL MESSAGE" << std::endl;
                } else {
                    std::cout << "GOT SOME OTHER MESSAGE: " << msg->type << std::endl;
                }
            });
        }
    }
    catch (std::exception ex) {
        std::cout << "GOT EXCEPTION!!! " << ex.what() << std::endl;
    }

}

std::shared_ptr<rtc::Track> RtspStream::createTrack(std::shared_ptr<rtc::PeerConnection> pc)

{
    const rtc::SSRC ssrc = 42;
    rtc::Description::Video media("video", rtc::Description::Direction::SendOnly);
    media.addH264Codec(96); // Must match the payload type of the external h264 RTP stream
    media.addSSRC(ssrc, "video-send");
    auto track = pc->addTrack(media);
    RtpTrack videoTrack = {
        pc,
        track,
        42,
        96,
        96
    };
    std::cout << "adding track with pt 96->" << 96 << std::endl;
    // TODO: thread safety
    videoTracks_.push_back(videoTrack);
    if (stopped_) {
        start();
        auto self = shared_from_this();
        track->onMessage([self](rtc::message_variant data) {
            // TODO: implement RTCP forwarding
            auto msg = rtc::make_message(data);
            if (msg->type == rtc::Message::Control) {
                std::cout << "GOT CONTROL MESSAGE" << std::endl;
            } else {
                std::cout << "GOT SOME OTHER MESSAGE type: " << msg->type << " size: " << msg->size() << std::endl;
                for (auto i : *msg) {
                    std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)i;
                }
                std::cout << std::dec << std::endl;
                std::byte* data = msg->data();

                // TODO: proper rtcp parsing for multiple payloads
                auto rtp = reinterpret_cast<rtc::RtcpRr*>(data);
                rtp->setSenderSSRC(self->rtcpSenderSsrc_);

                struct sockaddr_in addr = {};
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = inet_addr(self->videoHost_.c_str());
                addr.sin_port = htons(self->ctrlPort_);

                auto ret = sendto(self->rtcpSock_, data, msg->size(), 0, (struct sockaddr*)&addr, sizeof(addr));
                // ssize_t ret = write(self->rtcpSock_, data, msg->size());
                std::cout << "Wrote " << ret << " bytes to socket" << std::endl;

            }
        });
    }
    return track;
}

void RtspStream::removeConnection(std::shared_ptr<rtc::PeerConnection> pc)
{
    std::cout << "Removing PeerConnection from RTP" << std::endl;
    for (std::vector<RtpTrack>::iterator it = videoTracks_.begin(); it != videoTracks_.end(); it++) {
        if (pcPtrComp(it->pc, pc)) {
            std::cout << "Found PeerConnection" << std::endl;
            // TODO: thread safety
            videoTracks_.erase(it);
            break;
        }
    }
    if (videoTracks_.size() == 0) {
        std::cout << "PeerConnection was last one. Stopping" << std::endl;
        stop();
    } else {
        std::cout << "Still " << videoTracks_.size() << " PeerConnections. Not stopping" << std::endl;
    }
}

std::string RtspStream::getTrackId()
{
    return trackId_;
}

void RtspStream::start()
{
    stopped_ = false;
    videoRtpSock_ = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(videoHost_.c_str());
    addr.sin_port = htons(videoPort_);
    if (bind(videoRtpSock_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::string err = "Failed to bind UDP socket on " + videoHost_ + ":";
        err += videoPort_;
        std::cout << err << std::endl;
        throw std::runtime_error(err);
    }

    int rcvBufSize = 212992;
    setsockopt(videoRtpSock_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvBufSize),
        sizeof(rcvBufSize));
    videoThread_ = std::thread(streamRunner, this);

    rtcpSock_ = socket(AF_INET, SOCK_DGRAM, 0);
    addr.sin_port = htons(ctrlPort_);
    if (bind(rtcpSock_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::string err = "Failed to bind UDP socket on " + videoHost_ + ":";
        err += ctrlPort_;
        std::cout << err << std::endl;
        throw std::runtime_error(err);
    }

    setsockopt(rtcpSock_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvBufSize),
        sizeof(rcvBufSize));
    ctrlThread_ = std::thread(ctrlRunner, this);
}

void RtspStream::stop()
{
    std::cout << "RtspStream stopped" << std::endl;
    stopped_ = true;
    if (videoRtpSock_ != 0) {
        close(videoRtpSock_);
    }
    if (rtcpSock_ != 0) {
        close(rtcpSock_);
    }
    videoThread_.join();
    std::cout << "RtspStream thread joined" << std::endl;
    ctrlThread_.join();
    std::cout << "Rtcp thread joined" << std::endl;
}

void RtspStream::streamRunner(RtspStream* self)
{
    char buffer[RTP_BUFFER_SIZE];
    int len;
    int count = 0;
    while ((len = recv(self->videoRtpSock_, buffer, RTP_BUFFER_SIZE, 0)) >= 0 && !self->stopped_) {
        count++;
        if (count % 100 == 0) {
            std::cout << ".";
        }
        if (count % 1600 == 0) {
            std::cout << std::endl;
            count = 0;
        }
        // TODO: thread safety
        if (len < sizeof(rtc::RtpHeader)) {
            continue;
        }
        auto rtp = reinterpret_cast<rtc::RtpHeader*>(buffer);
        for (auto t : self->videoTracks_) {
            if (!t.track || !t.track->isOpen()) {
                // std::cout << "Invalid track not forwarding. isOpen: " << t.track->isOpen() << std::endl;
                continue;
            }
            rtp->setSsrc(t.ssrc);
            rtp->setPayloadType(t.dstPayloadType);
            t.track->send(reinterpret_cast<const std::byte*>(buffer), len);
        }

    }

}

void RtspStream::ctrlRunner(RtspStream* self)
{
    uint8_t buffer[RTP_BUFFER_SIZE];
    int len;
    int count = 0;
    while ((len = recv(self->rtcpSock_, buffer, RTP_BUFFER_SIZE, 0)) >= 0 && !self->stopped_) {
        // TODO: thread safety
        // TODO: proper rtcp parsing for more than 2 payloads
        if (len < sizeof(rtc::RtcpRr)) {
            continue;
        }
        auto rtp = reinterpret_cast<rtc::RtcpRr*>(buffer);
        uint16_t rtpLen = rtp->header.length();
        rtpLen = (rtpLen+1)*4;

        rtc::RtcpRr* rtp2 = NULL;
        if (rtpLen < len) {
            rtp2 = reinterpret_cast<rtc::RtcpRr*>(buffer + rtpLen);
        }

        self->rtcpSenderSsrc_ = rtp->senderSSRC();

        for (auto t : self->videoTracks_) {
            if (!t.track || !t.track->isOpen()) {
                continue;
            }
            std::cout << ":";
            rtp->setSenderSSRC(t.ssrc);
            if (rtp2 != NULL) {
                rtp2->setSenderSSRC(t.ssrc);
            }

            // for (uint8_t* i = buffer; i < buffer+len; i++) {
            //     std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)(*i);
            // }
            // std::cout << std::dec << std::endl;

            t.track->send(reinterpret_cast<const std::byte*>(buffer), len);
        }

    }

}

} // namespace
