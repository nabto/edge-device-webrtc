
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


void RtspStream::addTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc, std::string trackId)
{
    if (trackId.find(trackId_) != 0) {
        std::cout << "RtspStream got addTrack from wrong track ID: " << trackId << " != " << trackId_ << std::endl;
        return;
    }

    RtspConnection conn;
    conn.client = RtspClient::create(trackId_, url_);
    // TODO: maybe ephemeral ports
    conn.client->setRtpStartPort(42222 + (counter_ * 4));

    conn.pc = pc;
    connections_.push_back(conn);
    counter_++;
    size_t index = connections_.size() - 1;

    auto self = shared_from_this();
    conn.client->start([self, track, pc, trackId, index](CURLcode res) {
        if (res != CURLE_OK) {
            std::cout << "Failed to start RTSP client " << res << std::endl;
            return;
        }
        auto conn = self->connections_[index];
        auto video = conn.client->getVideoStream();
        if (video != nullptr && (self->trackId_ == trackId || trackId == self->trackId_ + "-video")) {
            // exact match or video
            video->addTrack(track, pc, self->trackId_ + "-video");

        }
        auto audio = conn.client->getAudioStream();
        if (audio != nullptr && (self->trackId_ == trackId || trackId == self->trackId_ + "-audio")) {
            // exact match or audio
            audio->addTrack(track, pc, self->trackId_ + "-audio");

        }
    });
}

void RtspStream::createTrack(std::shared_ptr<rtc::PeerConnection> pc)
{
    RtspConnection conn;
    size_t index = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        conn.client = RtspClient::create(trackId_, url_);
        // TODO: maybe ephemeral ports
        conn.client->setRtpStartPort(42222+(counter_*4));
        conn.pc = pc;
        connections_.push_back(conn);
        counter_++;

        index = connections_.size()-1;
    }
    auto self = shared_from_this();
    conn.client->start([self, pc, index](CURLcode res) {
        if (res != CURLE_OK) {
            std::cout << "Failed to start RTSP client " << res << std::endl;
            return;
        }
        {
            std::lock_guard<std::mutex> lock(self->mutex_);
            auto conn = self->connections_[index];
            auto video = conn.client->getVideoStream();
            if (video != nullptr) {
                video->createTrack(pc);

            }
            auto audio = conn.client->getAudioStream();
            if (audio != nullptr) {
                audio->createTrack(pc);
            }
            pc->setLocalDescription();
        }
    });
    return;
}

std::string RtspStream::getTrackId()
{
    return trackId_;
}

void RtspStream::removeConnection(std::shared_ptr<rtc::PeerConnection> pc)
{
    std::lock_guard<std::mutex> lock(mutex_);
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
