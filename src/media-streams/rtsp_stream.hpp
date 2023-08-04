#pragma once

#include "media_stream.hpp"
#include "rtp_track.hpp"

#include <sys/socket.h>
typedef int SOCKET;

#include <memory>
#include <thread>

namespace nabto {

class RtspStream;

typedef std::shared_ptr<RtspStream> RtspStreamPtr;

class RtspStream : public MediaStream,
                   public std::enable_shared_from_this<RtspStream>
{
public:
    static RtspStreamPtr create(std::string trackId);
    RtspStream(std::string& trackId);
    ~RtspStream();

    void addTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc);

    std::shared_ptr<rtc::Track> createTrack(std::shared_ptr<rtc::PeerConnection> pc);

    std::string getTrackId();

    void removeConnection(std::shared_ptr<rtc::PeerConnection> pc);




    // void addVideoTrack(RtcTrackPtr track, RtcPCPtr pc);
    // void addAudioTrack(RtcTrackPtr track, RtcPCPtr pc);
    // void removeConnection(RtcPCPtr pc);
    // std::string getVideoTrackId();
    // std::string getAudioTrackId();

    void setStreamPort(uint16_t port) { videoPort_ = port; }
    void setControlPort(uint16_t port) { ctrlPort_ = port; }
    void setVideoHost(std::string host) { videoHost_ = host; }


private:
    void start();
    void stop();
    static void streamRunner(RtspStream* self);
    static void ctrlRunner(RtspStream* self);
    static bool pcPtrComp(const RtcPCPtr& a, const RtcPCPtr& b) {
        if (a == b) return true;
        if (a && b) return a.get() == b.get();
        return false;
    };

    std::string trackId_;
    bool stopped_ = true;

    // TODO: duplicate for audio
    std::vector<RtpTrack> videoTracks_;
    uint16_t videoPort_ = 6000;
    uint16_t ctrlPort_ = 6001;
    std::string videoHost_ = "127.0.0.1";
    SOCKET videoRtpSock_ = 0;
    SOCKET rtcpSock_ = 0;
    std::thread videoThread_;
    std::thread ctrlThread_;
    rtc::SSRC rtcpSenderSsrc_ = 0;

};


} // namespace
