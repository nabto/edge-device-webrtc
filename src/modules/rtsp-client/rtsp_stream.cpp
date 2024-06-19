
#include "rtsp_stream.hpp"

namespace nabto {

RtspClientConf RtspStream::buildClientConf(std::string trackId, uint16_t port)
{
    RtspClientConf conf = { trackId, config_.url, config_.videoNegotiator, config_.audioNegotiator, config_.videoRepack, config_.audioRepack, config_.preferTcp, port };
    return conf;
}

RtspStreamPtr RtspStream::create(const RtspStreamConf& conf)
{
    return std::make_shared<RtspStream>(conf);

}

RtspStream::RtspStream(const RtspStreamConf& conf)
    : config_(conf)
{
}

RtspStream::~RtspStream()
{
}

bool RtspStream::isTrack(const std::string& trackId)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (config_.trackIdBase + "-audio" == trackId ||
        config_.trackIdBase + "-video" == trackId) {
        return true;
    }
    return false;
}

bool RtspStream::matchMedia(MediaTrackPtr media)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (media->getTrackId() == config_.trackIdBase + "-audio") {
        int pt = config_.audioNegotiator->match(media);
        if (pt == 0) {
            NPLOGE << "    Audio codec matching Failed. Audio will not work.";
            return false;
        }
        return true;
    }
    else if (media->getTrackId() == config_.trackIdBase + "-video") {
        int pt = config_.videoNegotiator->match(media);
        if (pt == 0) {
            NPLOGE << "    Video codec matching failed. Video will not work";
            return false;
        }
        return true;
    } else {
        NPLOGE << "MatchMedia called with invalid track ID";
        return false;
    }
}

void RtspStream::addConnection(NabtoDeviceConnectionRef ref, MediaTrackPtr media)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto conn = connections_.find(ref);
    if (conn == connections_.end()) {
        RtspConnection rtsp;
        RtspClientConf conf = buildClientConf(media->getTrackId(), 42222 + (counter_ * 4));
        rtsp.client = RtspClient::create(conf);

        connections_[ref] = rtsp;
        counter_++;
        auto self = shared_from_this();
        rtsp.client->start([self, ref](std::optional<std::string> error) {
            if (error.has_value()) {
                NPLOGE << "Failed to start RTSP client with error: " << error.value();
                return;
            }
            std::lock_guard<std::mutex> lock(self->mutex_);
            try {
                auto conn = self->connections_.at(ref);
                conn.client->addConnection(ref, conn.videoTrack, conn.audioTrack);
            } catch (std::out_of_range& ex) {
                NPLOGE << "RTSP client start callback received on closed connection";
            }
        });
        conn = connections_.find(ref);
    }
    if (media->getTrackId() == config_.trackIdBase + "-audio") {
        conn->second.audioTrack = media;
    }
    else if (media->getTrackId() == config_.trackIdBase + "-video") {
        conn->second.videoTrack = media;
    }
    else {
        NPLOGE << "addConnection called with invalid track ID";
    }
}

void RtspStream::removeConnection(NabtoDeviceConnectionRef ref)
{
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        auto conn = connections_.at(ref);
        conn.client->removeConnection(ref);
        conn.client->stop();
        connections_.erase(ref);
    } catch (std::out_of_range& ex) {
        // main makes this call for both video and audio, so this is just the second call where the connection is already removed.
    }
}

} // namespace
