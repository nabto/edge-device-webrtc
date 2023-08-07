#pragma once

#include "media_stream.hpp"
#include "rtp_track.hpp"

#include <sys/socket.h>
typedef int SOCKET;

#include <memory>
#include <thread>

namespace nabto {

class RtpClient;

typedef std::shared_ptr<RtpClient> RtpClientPtr;

class RtpClient : public MediaStream,
                  public std::enable_shared_from_this<RtpClient>
{
public:
    static RtpClientPtr create(std::string trackId);
    RtpClient(std::string& trackId);
    ~RtpClient();

    void addTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc);

    std::shared_ptr<rtc::Track> createTrack(std::shared_ptr<rtc::PeerConnection> pc);

    std::string getTrackId();

    void removeConnection(std::shared_ptr<rtc::PeerConnection> pc);

    void setVideoPort(uint16_t port) { videoPort_ = port; }
    void setVideoHost(std::string host) { videoHost_ = host; }


private:
    void start();
    void stop();
    static void rtpVideoRunner(RtpClient* self);
    static bool pcPtrComp(const RtcPCPtr& a, const RtcPCPtr& b) {
        if (a == b) return true;
        if (a && b) return a.get() == b.get();
        return false;
    };

    std::string trackId_;
    bool stopped_ = true;

    std::vector<RtpTrack> videoTracks_;
    uint16_t videoPort_ = 6000;
    std::string videoHost_ = "127.0.0.1";
    SOCKET videoRtpSock_ = 0;
    std::thread videoThread_;

};


} // namespace
