#pragma once

#include "media_stream.hpp"
#include "rtp_track.hpp"

#include <curl/curl.h>
#include <sys/socket.h>
typedef int SOCKET;

#include <memory>
#include <thread>

namespace nabto {

const int RTSP_BUFFER_SIZE = 2048;

class RtspClient;

typedef std::shared_ptr<RtspClient> RtspClientPtr;

class RtspClient : public MediaStream,
                   public std::enable_shared_from_this<RtspClient>
{
public:
    static RtspClientPtr create(std::string trackId, std::string& url);
    RtspClient(std::string& trackId, std::string& url);
    ~RtspClient();

    void addVideoTrack(RtcTrackPtr track, RtcPCPtr pc);
    void addAudioTrack(RtcTrackPtr track, RtcPCPtr pc);
    void removeConnection(RtcPCPtr pc);
    std::string getVideoTrackId();
    std::string getAudioTrackId();

private:
    std::string trackId_;
    std::string url_;
    bool stopped_ = false;

    CURL* curl_;

    // TODO: duplicate for audio
    std::vector<RtpTrack> videoTracks_;
    uint16_t videoPort_ = 6000;
    std::string videoHost_ = "127.0.0.1";
    SOCKET videoRtpSock_ = 0;
    std::thread videoThread_;

};


} // namespace
