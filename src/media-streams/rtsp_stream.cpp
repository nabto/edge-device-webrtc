
#include "rtsp_stream.hpp"

namespace nabto {

RtspStreamPtr RtspStream::create(std::string trackId, std::string& url)
{
    return std::make_shared<RtspStream>(trackId, url);

}

RtspStream::RtspStream(std::string& trackId, std::string& url)
    : trackId_(trackId), url_(url)
{

}

RtspStream::~RtspStream()
{
    std::cout << "RtspStream destructor" << std::endl;
}


void RtspStream::addTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc)
{
    RtspConnection conn;
    conn.client = RtspClient::create(trackId_, url_);
    // TODO: maybe ephemeral ports
    conn.client->setRtpStartPort(42222 + (counter_ * 4));
    conn.client->start();

    auto video = conn.client->getVideoStream();
    if (video != nullptr) {
        video->addTrack(track, pc);
    }
    auto audio = conn.client->getAudioStream();
    if (audio != nullptr) {
        audio->addTrack(track, pc);
    }
    conn.pc = pc;
    connections_.push_back(conn);
    counter_++;
}

void RtspStream::createTrack(std::shared_ptr<rtc::PeerConnection> pc)
{

    RtspConnection conn;
    conn.client = RtspClient::create(trackId_, url_);
    // TODO: maybe ephemeral ports
    conn.client->setRtpStartPort(42222+(counter_*4));
    conn.client->start();

    auto video = conn.client->getVideoStream();
    if (video != nullptr) {
        video->createTrack(pc);
    }
    auto audio = conn.client->getAudioStream();
    if (audio != nullptr) {
        audio->createTrack(pc);
    }
    conn.pc = pc;
    connections_.push_back(conn);
    counter_++;
    return;
}

std::string RtspStream::getTrackId()
{
    return trackId_;
}

void RtspStream::removeConnection(std::shared_ptr<rtc::PeerConnection> pc)
{
    for (std::vector<RtspConnection>::iterator it = connections_.begin(); it != connections_.end(); it++) {
        if (pcPtrComp(it->pc, pc)) {
            std::cout << "RTSP Stream Found PeerConnection" << std::endl;
            auto vid = it->client->getVideoStream();
            if (vid != nullptr) {
                vid->removeConnection(pc);
                std::cout << "RTSP Stream conn removed from Video" << std::endl;
            }
            auto au = it->client->getAudioStream();
            if (au != nullptr) {
                au->removeConnection(pc);
                std::cout << "RTSP Stream conn removed from audio" << std::endl;
            }
            std::cout << "stopping RTSP client" << std::endl;
            it->client->stop();
            connections_.erase(it);
            break;
        }
    }
}



} // namespace
