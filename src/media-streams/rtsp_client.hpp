#pragma once

#include "media_stream.hpp"
#include "rtp_client.hpp"
#include "rtcp_client.hpp"
#include <util.hpp>

#include <rtc/description.hpp>

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

    bool start(std::function<void(CURLcode res)> cb);
    void stop();

    MediaStreamPtr getVideoStream();
    MediaStreamPtr getAudioStream();

    // Sets which transport ports to make RTSP server send RTP traffic to.
    // 4 ports are used in total:
    //   port  : Port for video stream
    //   port+1: Port for video RTCP
    //   port+2: Port for Audio stream if exists
    //   port+3: Port for Audio RTCP if exists
    // if unset port defaults to 42222 meaning 42222-42225 is used.
    void setRtpStartPort(uint16_t port) { port_ = port;}

private:
    void setupRtsp();
    bool rtspPlay();
    void teardown();
    bool performSetupReq(std::string& url, std::string& transport);

    bool parseSdpDescription(std::string& desc);
    std::string parseControlAttribute(std::string& att);

    static size_t writeFunc(void* ptr, size_t size, size_t nmemb, void* self);

    void resolveStart(CURLcode res);

    std::string trackId_;
    std::string url_;
    uint16_t port_ = 42222;
    bool stopped_ = false;

    std::function<void(CURLcode res)> startCb_;

    CurlAsyncPtr curl_;
    std::string curlHeaders_;
    std::string readBuffer_;
    std::string contentBase_;

    std::string sessionControlUrl_;

    RtpClientPtr videoStream_ = nullptr;
    H264CodecMatcher videoCodec_;
    std::string videoControlUrl_;
    int videoPayloadType_;
    RtcpClientPtr videoRtcp_ = nullptr;

    RtpClientPtr audioStream_ = nullptr;
    // OpusCodecMatcher audioCodec_;
    PcmuCodecMatcher audioCodec_;
    std::string audioControlUrl_;
    int audioPayloadType_;
    RtcpClientPtr audioRtcp_ = nullptr;

};


} // namespace
