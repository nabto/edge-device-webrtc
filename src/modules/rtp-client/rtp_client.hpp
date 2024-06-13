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

class RtpClient : public MediaStream, public std::enable_shared_from_this<RtpClient>
{
public:
    static RtpClientPtr create(const std::string& trackId);
    RtpClient(const std::string& trackId);
    ~RtpClient();

    bool isTrack(const std::string& trackId);
    void addConnection(NabtoDeviceConnectionRef ref, MediaTrackPtr media);
    void removeConnection(NabtoDeviceConnectionRef ref);
    bool matchMedia(MediaTrackPtr media);

    void setPort(uint16_t port) { videoPort_ = port; remotePort_ = port + 1; }
    // Remote Host used to send data to if stream is not SENDONLY
    void setRemoteHost(std::string host) { remoteHost_ = host; }
    void setTrackNegotiator(TrackNegotiatorPtr negotiator) { negotiator_ = negotiator; }
    TrackNegotiatorPtr getTrackNegotiator() { return negotiator_; }

    MediaTrackPtr createMedia(const std::string& trackId) {
        auto m = negotiator_->createMedia();
        m.addSSRC(negotiator_->ssrc(), trackId_);
        auto sdp = m.generateSdp();
        return MediaTrack::create(trackId, sdp);
    }

    std::string getTrackId();

    void setRepacketizerFactory(RtpRepacketizerFactoryPtr repack)
    {
        if (repack == nullptr) {
            repack_ = RtpRepacketizerFactory::create();
        }
        else {
            repack_ = repack;
        }
    }

private:
    void start();
    void stop();
    void addConnection(NabtoDeviceConnectionRef ref, RtpTrack track);
    static void rtpVideoRunner(RtpClient* self);

    std::string trackId_;
    bool stopped_ = true;
    std::mutex mutex_;

    std::map<NabtoDeviceConnectionRef, RtpTrack> mediaTracks_;

    uint16_t videoPort_ = 6000;
    uint16_t remotePort_ = 6002;
    std::string remoteHost_ = "127.0.0.1";
    SOCKET videoRtpSock_ = 0;
    std::thread videoThread_;
    TrackNegotiatorPtr negotiator_;
    RtpRepacketizerFactoryPtr repack_ = RtpRepacketizerFactory::create();

};


} // namespace
