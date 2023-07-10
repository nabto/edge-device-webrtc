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

    std::vector<RtpTrack> videoTracks_;
//    std::map<RtcPCPtr, RtcTrackPtr, decltype(pcPtrComp)> videoTracks_;
    // std::shared_ptr<rtc::Track> track_;
    // rtc::SSRC ssrc_ = 42;
    // int srcPayloadType_ = 0;
    // int dstPayloadType_ = 0;

    SOCKET sock_ = 0;
    bool stopped_ = true;
    std::thread streamThread_;
};


} // namespace
