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

void RtpStream::addTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc, std::string trackId)
{
    if (trackId.find(trackId_) == 0) {
        // incoming trackId contains our track ID
        // eg. our ID is frontdoor, and incoming is frontdoor, frontdoor-audio, or frontdoor-video
        if (videoClient_ != nullptr && (trackId_ == trackId || trackId == trackId_ + "-video")) {
            // exact match or video
            videoClient_->addTrack(track, pc, trackId_ + "-video");
        }
        if (audioClient_ != nullptr && (trackId_ == trackId || trackId == trackId_ + "-audio")) {
            // exact match or audio
            audioClient_->addTrack(track, pc, trackId_ + "-audio");
        }
    }
}

void RtpStream::createTrack(std::shared_ptr<rtc::PeerConnection> pc)

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
    pc->setLocalDescription();
    return;
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

// TODO: impl sdp();
std::string RtpStream::sdp() { return ""; }


} // namespace
