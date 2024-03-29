#include "rtp_client.hpp"

#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <iomanip> // For std::setfill and std::setw

const int RTP_BUFFER_SIZE = 2048;


namespace nabto {


RtpClientPtr RtpClient::create(const std::string& trackId)
{
    return std::make_shared<RtpClient>(trackId);
}

RtpClient::RtpClient(const std::string& trackId)
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

bool RtpClient::isTrack(const std::string& trackId)
{
    if (trackId == trackId_) {
        return true;
    }
    return false;
}

bool RtpClient::matchMedia(MediaTrackPtr media)
{
    int pt = negotiator_->match(media);
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

    const rtc::SSRC ssrc = negotiator_->ssrc();
    RtpTrack track = {
        media,
        ssrc,
        negotiator_->payloadType(),
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

    if (negotiator_->direction() != TrackNegotiator::SEND_ONLY) {
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
    mediaTracks_.clear();
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
                }
            }
        }
    }
}

} // namespace
