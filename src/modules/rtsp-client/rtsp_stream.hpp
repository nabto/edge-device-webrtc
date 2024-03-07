#pragma once

#include "rtsp_client.hpp"

#include <media-streams/media_stream.hpp>
#include <nabto/nabto_device_webrtc.hpp>

#include <rtc/rtc.hpp>

namespace nabto {

class RtspStream;

typedef std::shared_ptr<RtspStream> RtspStreamPtr;

class RtspConnection
{
public:
    MediaTrackPtr videoTrack;
    MediaTrackPtr audioTrack;
    RtspClientPtr client;
};


class RtspStream : public MediaStream, public std::enable_shared_from_this<RtspStream>
{
public:
    static RtspStreamPtr create(const std::string& trackIdBase, const std::string& url);
    RtspStream(const std::string& trackIdBase, const std::string& url);
    ~RtspStream();

    void stop() {
        for (const auto& [key, value] : connections_) {
            value.client->stop();
        }
        connections_.clear();
    }

    bool isTrack(const std::string& trackId);
    bool matchMedia(MediaTrackPtr media);
    void addConnection(NabtoDeviceConnectionRef ref, MediaTrackPtr media);
    void removeConnection(NabtoDeviceConnectionRef ref);

    void setTrackNegotiators(TrackNegotiatorPtr videoNegotiator, TrackNegotiatorPtr audioNegotiator)
    {
        videoNegotiator_ = videoNegotiator;
        audioNegotiator_ = audioNegotiator;
    }

    void setPort(uint16_t port) { basePort_ = port; }

    MediaTrackPtr createMedia(const std::string& trackId) {
        if (trackId == trackIdBase_ + "-audio") {
            auto m = audioNegotiator_->createMedia();
            m.addSSRC(audioNegotiator_->ssrc(), trackId);
            auto sdp = m.generateSdp();
            return MediaTrack::create(trackId, sdp);

        }
        else if (trackId == trackIdBase_ + "-video") {
            auto m = videoNegotiator_->createMedia();
            m.addSSRC(videoNegotiator_->ssrc(), trackId);
            auto sdp = m.generateSdp();
            return MediaTrack::create(trackId, sdp);
        }
        else {
            std::cout << "crateMedia called with invalid track ID" << std::endl;
            return nullptr;
        }

    }

private:

    std::string trackIdBase_;
    std::string url_;

    std::mutex mutex_;
    size_t counter_ = 0;

    TrackNegotiatorPtr videoNegotiator_;
    TrackNegotiatorPtr audioNegotiator_;
    uint16_t basePort_;

    std::map<NabtoDeviceConnectionRef, RtspConnection> connections_;
};

} // namespace
