#pragma once

#include "media_stream.hpp"

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

    void addVideoTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc);
    void addAudioTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc);
    void removeConnection(std::shared_ptr<rtc::PeerConnection> pc);
    std::string getVideoTrackId();
    std::string getAudioTrackId();


private:
    void start();
    void stop();
    static void rtpRunner(RtpClient* self);

    std::string trackId_;
    std::shared_ptr<rtc::Track> track_;
    rtc::SSRC ssrc_ = 42;
    int srcPayloadType_ = 0;
    int dstPayloadType_ = 0;

    SOCKET sock_ = 0;
    bool stopped_ = false;
    std::thread streamThread_;
};


} // namespace
