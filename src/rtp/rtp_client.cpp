#include "rtp_client.hpp"

#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>

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


void RtpClient::addVideoTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc)
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

        srcPayloadType_ = 96;
        dstPayloadType_ = rtp->payloadType;

        media.addSSRC(ssrc_, trackId_+"-video");
        // TODO: add tracks to a list along with their pc
        track_ = pc->addTrack(media);
        start();
    }
    catch (std::exception ex) {
        std::cout << "GOT EXCEPTION!!! " << ex.what() << std::endl;
    }

}

void RtpClient::addAudioTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc)
{
    // TODO: implement
}

void RtpClient::removeConnection(std::shared_ptr<rtc::PeerConnection> pc)
{
    // TODO: only stop if pc is last in list
    stop();
}

std::string RtpClient::getVideoTrackId()
{
    return trackId_ + "-video";
}

std::string RtpClient::getAudioTrackId()
{
    return trackId_ + "-audio";
}

void RtpClient::start()
{
    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(6000);
    if (bind(sock_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::string err = "Failed to bind UDP socket on 127.0.0.1:" + 6000;
        throw std::runtime_error(err);
    }

    int rcvBufSize = 212992;
    setsockopt(sock_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvBufSize),
        sizeof(rcvBufSize));
    streamThread_ = std::thread(rtpRunner, this);
}

void RtpClient::stop()
{
    stopped_ = true;
    if (sock_ != 0) {
        close(sock_);
    }
    streamThread_.join();
}

void RtpClient::rtpRunner(RtpClient* self)
{
    char buffer[RTP_BUFFER_SIZE];
    int len;
    int count = 0;
    while ((len = recv(self->sock_, buffer, RTP_BUFFER_SIZE, 0)) >= 0 && !self->stopped_) {
        count++;
        if (count % 100 == 0) {
            std::cout << ".";
        }
        if (count % 1600 == 0) {
            std::cout << std::endl;
            count = 0;
        }
        if (len < sizeof(rtc::RtpHeader) || !self->track_ || !self->track_->isOpen())
            continue;

        auto rtp = reinterpret_cast<rtc::RtpHeader*>(buffer);
        rtp->setSsrc(self->ssrc_);
        rtp->setPayloadType(self->dstPayloadType_);

        self->track_->send(reinterpret_cast<const std::byte*>(buffer), len);
    }

}

} // namespace
