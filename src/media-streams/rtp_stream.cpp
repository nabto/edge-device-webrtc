#include "rtp_stream.hpp"


namespace nabto {


RtpStreamPtr RtpStream::create(std::string trackId)
{
    return std::make_shared<RtpStream>(trackId);
}

RtpStream::RtpStream(std::string& trackId)
    : trackId_(trackId)
{

}

RtpStream::~RtpStream()
{
    std::cout << "RtpStream destructor" << std::endl;
}


void RtpStream::setVideoConf(uint16_t port, RtpCodec* matcher, std::string host)
{
    videoClient_ = RtpClient::create(trackId_ + "-video");
    videoClient_->setPort(port);
    videoClient_->setRtpCodecMatcher(matcher);
    videoClient_->setRemoteHost(host);
}

void RtpStream::setAudioConf(uint16_t port, RtpCodec* matcher, std::string host)
{
    audioClient_ = RtpClient::create(trackId_ + "-audio");
    audioClient_->setPort(port);
    audioClient_->setRtpCodecMatcher(matcher);
    audioClient_->setRemoteHost(host);

}

void RtpStream::addTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc)
{
    if (videoClient_ != nullptr) {
        videoClient_->addTrack(track, pc);
    }
    if (audioClient_ != nullptr) {
        audioClient_->addTrack(track, pc);
    }
}

std::shared_ptr<rtc::Track> RtpStream::createTrack(std::shared_ptr<rtc::PeerConnection> pc)

{
    std::cout << "RTP Stream Create track. ";
    if (videoClient_ != nullptr) {
        std::cout << "Creating video Track. ";
        videoClient_->createTrack(pc);
    }
    if (audioClient_ != nullptr) {
        std::cout << "Creating audio Track. ";
        audioClient_->createTrack(pc);
    }
    std::cout << std::endl;
    return nullptr;
}

void RtpStream::removeConnection(std::shared_ptr<rtc::PeerConnection> pc)
{
    if (videoClient_ != nullptr) {
        videoClient_->removeConnection(pc);
    }
    if (audioClient_ != nullptr) {
        audioClient_->removeConnection(pc);
    }
}

std::string RtpStream::getTrackId()
{
    return trackId_;
}

} // namespace
