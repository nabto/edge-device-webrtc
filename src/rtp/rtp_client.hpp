#pragma once

#include "media_stream.hpp"

#include <sys/socket.h>
typedef int SOCKET;

#include <memory>
#include <thread>

namespace nabto {

typedef std::shared_ptr<rtc::Track> RtcTrackPtr;
typedef std::shared_ptr<rtc::PeerConnection> RtcPCPtr;

class RtpClient;

class RtpTrack
{
public:
    RtcPCPtr pc;
    RtcTrackPtr track;
    rtc::SSRC ssrc;
    int srcPayloadType = 0;
    int dstPayloadType = 0;
};

typedef std::shared_ptr<RtpClient> RtpClientPtr;

class RtpClient : public MediaStream,
                  public std::enable_shared_from_this<RtpClient>
{
public:
    static RtpClientPtr create(std::string trackId);
    RtpClient(std::string& trackId);
    ~RtpClient();

    void addVideoTrack(RtcTrackPtr track, RtcPCPtr pc);
    void addAudioTrack(RtcTrackPtr track, RtcPCPtr pc);
    void removeConnection(RtcPCPtr pc);
    std::string getVideoTrackId();
    std::string getAudioTrackId();

    void setVideoPort(uint16_t port) { videoPort_ = port; }
    void setVideoHost(std::string host) { videoHost_ = host; }


private:
    void start();
    void stop();
    static void rtpRunner(RtpClient* self);
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
    std::string videoHost_ = "127.0.0.1";
    SOCKET sock_ = 0;
    std::thread streamThread_;

};


} // namespace
