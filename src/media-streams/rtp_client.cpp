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

RtpClient::~RtpClient()
{
    std::cout << "RtpClient destructor" << std::endl;
}

void RtpClient::addVideoTrack(RtcTrackPtr track, RtcPCPtr pc)
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
        }
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
    videoThread_ = std::thread(rtpVideoRunner, this);
}

void RtpClient::stop()
{
    std::cout << "RtpClient stopped" << std::endl;
    stopped_ = true;
    if (videoRtpSock_ != 0) {
        close(videoRtpSock_);
    }
    videoThread_.join();
    std::cout << "RtpClient thread joined" << std::endl;
}

void RtpClient::rtpVideoRunner(RtpClient* self)
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
                continue;
            }
            rtp->setSsrc(t.ssrc);
            rtp->setPayloadType(t.dstPayloadType);
            t.track->send(reinterpret_cast<const std::byte*>(buffer), len);
        }

    }

}

} // namespace
