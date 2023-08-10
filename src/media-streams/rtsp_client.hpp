#pragma once

#include "media_stream.hpp"
#include "rtp_client.hpp"

#include <curl/curl.h>
#include <sys/socket.h>
typedef int SOCKET;

#include <memory>
#include <thread>

namespace nabto {

const int RTSP_BUFFER_SIZE = 2048;

class RtspClient;

typedef std::shared_ptr<RtspClient> RtspClientPtr;

class RtspClient : public std::enable_shared_from_this<RtspClient>
{
public:
    static RtspClientPtr create(std::string trackId, std::string& url);
    RtspClient(std::string& trackId, std::string& url);
    ~RtspClient();

    void start();
    void stop();

    MediaStreamPtr getVideoStream();
    MediaStreamPtr getAudioStream();

private:
    bool setupCurl();
    bool setupRtsp();
    bool rtspPlay();

    std::string trackId_;
    std::string url_;
    bool stopped_ = false;

    CURL* curl_;

    RtpClientPtr videoStream_ = nullptr;
    RtpClientPtr audioStream_ = nullptr;
};


} // namespace
