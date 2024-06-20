#pragma once

#include "rtsp_client.hpp"

#include <rtp-repacketizer/rtp_repacketizer.hpp>

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

class RtspStreamConf {
public:
    std::string trackIdBase;
    std::string url;
    TrackNegotiatorPtr videoNegotiator;
    TrackNegotiatorPtr audioNegotiator;
    RtpRepacketizerFactoryPtr videoRepack;
    RtpRepacketizerFactoryPtr audioRepack;
    bool preferTcp = true;
};

class RtspStream : public MediaStream, public std::enable_shared_from_this<RtspStream>
{
public:
    static RtspStreamPtr create(const RtspStreamConf& conf);
    RtspStream(const RtspStreamConf& conf);

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

    MediaTrackPtr createMedia(const std::string& trackId) {
        if (trackId == config_.trackIdBase + "-audio") {
            auto m = config_.audioNegotiator->createMedia();
            m.addSSRC(config_.audioNegotiator->ssrc(), trackId);
            auto sdp = m.generateSdp();
            return MediaTrack::create(trackId, sdp);

        }
        else if (trackId == config_.trackIdBase + "-video") {
            auto m = config_.videoNegotiator->createMedia();
            m.addSSRC(config_.videoNegotiator->ssrc(), trackId);
            auto sdp = m.generateSdp();
            return MediaTrack::create(trackId, sdp);
        }
        else {
            NPLOGE << "crateMedia called with invalid track ID";
            return nullptr;
        }

    }

private:
    RtspClientConf buildClientConf(std::string trackId, uint16_t port);
    RtspStreamConf config_;

    std::mutex mutex_;
    size_t counter_ = 0;

    std::map<NabtoDeviceConnectionRef, RtspConnection> connections_;
};

} // namespace
