
#include "rtsp_stream.hpp"

namespace nabto {

RtspStreamPtr RtspStream::create(const std::string& trackIdBase, const std::string& url)
{
    return std::make_shared<RtspStream>(trackIdBase, url);

}

RtspStream::RtspStream(const std::string& trackIdBase, const std::string& url)
    : trackIdBase_(trackIdBase), url_(url)
{

}

RtspStream::~RtspStream()
{
    std::cout << "RtspStream destructor" << std::endl;
}

bool RtspStream::isTrack(const std::string& trackId)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (trackIdBase_+"-audio" == trackId ||
    trackIdBase_+"-video" == trackId) {
        return true;
    }
    return false;
}

bool RtspStream::matchMedia(MediaTrackPtr media)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (media->getTrackId() == trackIdBase_ + "-audio") {
        int pt = audioNegotiator_->match(media);
        if (pt == 0) {
            std::cout << "    AUDIO CODEC MATCHING FAILED!!! " << std::endl;
            // TODO: Fail
            return false;
        }
        return true;
    }
    else if (media->getTrackId() == trackIdBase_ + "-video") {
        int pt = videoNegotiator_->match(media);
        if (pt == 0) {
            std::cout << "    VIDEO CODEC MATCHING FAILED!!! " << std::endl;
            // TODO: Fail
            return false;
        }
        return true;
    } else {
        std::cout << "MatchMedia called with invalid track ID" << std::endl;
        return false;
    }
}

void RtspStream::addConnection(NabtoDeviceConnectionRef ref, MediaTrackPtr media)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto conn = connections_.find(ref);
    if (conn == connections_.end()) {
        RtspConnection rtsp;
        rtsp.client = RtspClient::create(media->getTrackId(), url_);
        rtsp.client->setRtpStartPort(42222 + (counter_ * 4));
        rtsp.client->setTrackNegotiators(videoNegotiator_, audioNegotiator_);

        connections_[ref] = rtsp;
        counter_++;
        auto self = shared_from_this();
        rtsp.client->start([self, ref](CURLcode res) {
            if (res != CURLE_OK) {
                std::cout << "Failed to start RTSP client " << res << std::endl;
                return;
            }
            std::lock_guard<std::mutex> lock(self->mutex_);
            try {
                auto conn = self->connections_.at(ref);

                auto video = conn.client->getVideoStream();
                if (video != nullptr && conn.videoTrack != nullptr) {
                    video->addConnection(ref, conn.videoTrack);
                }

                auto audio = conn.client->getAudioStream();
                if (audio != nullptr && conn.audioTrack != nullptr) {
                    audio->addConnection(ref, conn.audioTrack);
                }
            } catch (std::out_of_range& ex) {
                std::cout << "RTSP client start callback received on closed connection" << std::endl;
            }
        });
        conn = connections_.find(ref);
    }
    if (media->getTrackId() == trackIdBase_ + "-audio") {
        conn->second.audioTrack = media;
    }
    else if (media->getTrackId() == trackIdBase_ + "-video") {
        conn->second.videoTrack = media;
    }
    else {
        std::cout << "addConnection called with invalid track ID" << std::endl;
    }
}

void RtspStream::removeConnection(NabtoDeviceConnectionRef ref)
{
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto conn = connections_.at(ref);
        auto v = conn.client->getVideoStream();
        if (v != nullptr) {
            v->removeConnection(ref);
        }
        auto a = conn.client->getAudioStream();
        if (a != nullptr) {
            a->removeConnection(ref);
        }
        conn.client->stop();
        connections_.erase(ref);
    } catch (std::out_of_range& ex) {
        // main makes this call for both video and audio, so this is just the second call where the connection is already removed.
    }
}

} // namespace
