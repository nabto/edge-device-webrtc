#pragma once

#include "rtp_track.hpp"

#include <media-streams/media_stream.hpp>
#include <track-negotiators/track_negotiator.hpp>
#include <rtp-repacketizer/rtp_repacketizer.hpp>
#include <sys/socket.h>

typedef int SOCKET;

#include <memory>
#include <thread>

namespace nabto {

class RtpClient;
typedef std::shared_ptr<RtpClient> RtpClientPtr;

class RtpClientConf {
public:
    std::string trackId;
    std::string remoteHost;
    uint16_t port = 0;
    TrackNegotiatorPtr negotiator;
    RtpRepacketizerFactoryPtr repacketizer;
};

class RtpClient : public MediaStream, public std::enable_shared_from_this<RtpClient>
{
public:
    static RtpClientPtr create(const RtpClientConf& conf);
    RtpClient(const RtpClientConf& conf);

    ~RtpClient();

    bool isTrack(const std::string& trackId);
    void addConnection(const std::string& webrtcConnectionId, MediaTrackPtr media);
    void removeConnection(const std::string& webrtcConnectionId);
    bool matchMedia(MediaTrackPtr media);

    TrackNegotiatorPtr getTrackNegotiator() { return negotiator_; }

    MediaTrackPtr createMedia(const std::string& trackId) {
        auto m = negotiator_->createMedia();
        m.addSSRC(negotiator_->ssrc(), trackId_);
        auto sdp = m.generateSdp();
        return MediaTrack::create(trackId, sdp);
    }

    std::string getTrackId();

private:
    void start();
    void stop();
    void addConnection(const std::string& webrtcConnectionId, RtpTrack track);
    static void rtpVideoRunner(RtpClient* self);

    std::string trackId_;
    bool stopped_ = true;
    std::mutex mutex_;

    std::map<std::string, RtpTrack> mediaTracks_;

    uint16_t videoPort_ = 6000;
    uint16_t remotePort_ = 6002;
    std::string remoteHost_ = "127.0.0.1";
    SOCKET videoRtpSock_ = 0;
    std::thread videoThread_;
    TrackNegotiatorPtr negotiator_;
    RtpRepacketizerFactoryPtr repack_ = RtpRepacketizerFactory::create();

};


} // namespace
