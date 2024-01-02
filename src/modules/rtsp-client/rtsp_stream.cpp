
#include "rtsp_stream.hpp"

namespace nabto {

RtspStreamPtr RtspStream::create(std::string trackIdBase, std::string& url)
{
    return std::make_shared<RtspStream>(trackIdBase, url);

}

RtspStream::RtspStream(std::string& trackIdBase, std::string& url)
    : trackIdBase_(trackIdBase), url_(url)
{

}

RtspStream::~RtspStream()
{
    std::cout << "RtspStream destructor" << std::endl;
}

bool RtspStream::isTrack(std::string& trackId)
{
    if (trackIdBase_+"-audio" == trackId ||
    trackIdBase_+"-video" == trackId) {
        return true;
    }
    return false;
}

bool RtspStream::matchMedia(MediaTrackPtr media)
{
    if (media->getTrackId() == trackIdBase_ + "-audio") {
        int pt = audioMatcher_->match(media);
        if (pt == 0) {
            std::cout << "    AUDIO CODEC MATCHING FAILED!!! " << std::endl;
            // TODO: Fail
            return false;
        }
        return true;
    }
    else if (media->getTrackId() == trackIdBase_ + "-video") {
        int pt = videoMatcher_->match(media);
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
    // TODO: mutex
    auto conn = connections_.find(ref);
    if (conn == connections_.end()) {
        // TODO: do not do this if track ID is invalid
        RtspConnection rtsp;
        rtsp.client = RtspClient::create(media->getTrackId(), url_);
        rtsp.client->setRtpStartPort(42222 + (counter_ * 4));

        connections_[ref] = rtsp;
        counter_++;
        auto self = shared_from_this();
        rtsp.client->start([self, ref](CURLcode res) {
            // TODO: This is called from curl worker thread, do mutex
            if (res != CURLE_OK) {
                std::cout << "Failed to start RTSP client " << res << std::endl;
                return;
            }
            // TODO: catch out_of_range ex
            auto conn = self->connections_.at(ref);

            auto video = conn.client->getVideoStream();
            if (video != nullptr && conn.videoTrack != nullptr) {
                video->addConnection(ref, conn.videoTrack);
            }

            auto audio = conn.client->getAudioStream();
            if (audio != nullptr && conn.audioTrack != nullptr) {
                audio->addConnection(ref, conn.audioTrack);
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

// void RtspStream::addTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc, std::string trackId)
// {
//     if (trackId.find(trackId_) != 0) {
//         std::cout << "RtspStream got addTrack from wrong track ID: " << trackId << " != " << trackId_ << std::endl;
//         return;
//     }

//     RtspConnection conn;
//     conn.client = RtspClient::create(trackId_, url_);
//     // TODO: maybe ephemeral ports
//     conn.client->setRtpStartPort(42222 + (counter_ * 4));

//     conn.pc = pc;
//     connections_.push_back(conn);
//     counter_++;
//     size_t index = connections_.size() - 1;

//     auto self = shared_from_this();
//     conn.client->start([self, track, pc, trackId, index](CURLcode res) {
//         if (res != CURLE_OK) {
//             std::cout << "Failed to start RTSP client " << res << std::endl;
//             return;
//         }
//         auto conn = self->connections_[index];
//         auto video = conn.client->getVideoStream();
//         if (video != nullptr && (self->trackId_ == trackId || trackId == self->trackId_ + "-video")) {
//             // exact match or video
//             video->addTrack(track, pc, self->trackId_ + "-video");

//         }
//         auto audio = conn.client->getAudioStream();
//         if (audio != nullptr && (self->trackId_ == trackId || trackId == self->trackId_ + "-audio")) {
//             // exact match or audio
//             audio->addTrack(track, pc, self->trackId_ + "-audio");

//         }
//     });
// }

// void RtspStream::createTrack(std::shared_ptr<rtc::PeerConnection> pc)
// {
//     RtspConnection conn;
//     size_t index = 0;
//     {
//         std::lock_guard<std::mutex> lock(mutex_);

//         conn.client = RtspClient::create(trackId_, url_);
//         // TODO: maybe ephemeral ports
//         conn.client->setRtpStartPort(42222+(counter_*4));
//         conn.pc = pc;
//         connections_.push_back(conn);
//         counter_++;

//         index = connections_.size()-1;
//     }
//     auto self = shared_from_this();
//     conn.client->start([self, pc, index](CURLcode res) {
//         if (res != CURLE_OK) {
//             std::cout << "Failed to start RTSP client " << res << std::endl;
//             return;
//         }
//         {
//             std::lock_guard<std::mutex> lock(self->mutex_);
//             auto conn = self->connections_[index];
//             auto video = conn.client->getVideoStream();
//             if (video != nullptr) {
//                 video->createTrack(pc);

//             }
//             auto audio = conn.client->getAudioStream();
//             if (audio != nullptr) {
//                 audio->createTrack(pc);
//             }
//             pc->setLocalDescription();
//         }
//     });
//     return;
// }

// std::string RtspStream::getTrackId()
// {
//     return trackId_;
// }

// void RtspStream::removeConnection(std::shared_ptr<rtc::PeerConnection> pc)
// {
//     std::lock_guard<std::mutex> lock(mutex_);
//     for (std::vector<RtspConnection>::iterator it = connections_.begin(); it != connections_.end(); it++) {
//         if (pcPtrComp(it->pc, pc)) {
//             std::cout << "RTSP Stream Found PeerConnection" << std::endl;
//             auto vid = it->client->getVideoStream();
//             if (vid != nullptr) {
//                 vid->removeConnection(pc);
//                 std::cout << "RTSP Stream conn removed from Video" << std::endl;
//             }
//             auto au = it->client->getAudioStream();
//             if (au != nullptr) {
//                 au->removeConnection(pc);
//                 std::cout << "RTSP Stream conn removed from audio" << std::endl;
//             }
//             std::cout << "stopping RTSP client" << std::endl;
//             it->client->stop();
//             connections_.erase(it);
//             break;
//         }
//     }
// }



} // namespace
