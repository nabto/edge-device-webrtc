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
    static RtspStreamPtr create(std::string trackIdBase, std::string& url);
    RtspStream(std::string& trackIdBase, std::string& url);
    ~RtspStream();

    void stop() {
        for (const auto& [key, value] : connections_) {
            value.client->stop();
        }
        connections_.clear();
    }

    bool isTrack(std::string& trackId);
    bool matchMedia(MediaTrackPtr media);
    void addConnection(NabtoDeviceConnectionRef ref, MediaTrackPtr media);
    void removeConnection(NabtoDeviceConnectionRef ref);

    void setCodecMatchers(RtpCodec* videoMatcher, RtpCodec* audioMatcher)
    {
        videoMatcher_ = videoMatcher;
        audioMatcher_ = audioMatcher;
    }

    void setPort(uint16_t port) { basePort_ = port; }

    // TODO: remove this
    std::string getTrackId() {
        return trackIdBase_;
    }

    // TODO: remove this
    std::string sdp() {
        auto m = videoMatcher_->createMedia();
        m.addSSRC(videoMatcher_->ssrc(), trackIdBase_ + "-video");
        return m.generateSdp();
    }

private:

    std::string trackIdBase_;
    std::string url_;

    std::mutex mutex_;
    size_t counter_ = 0;

    RtpCodec* videoMatcher_;
    RtpCodec* audioMatcher_;
    uint16_t basePort_;

    std::map<NabtoDeviceConnectionRef, RtspConnection> connections_;
};

} // namespace

