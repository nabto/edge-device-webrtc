#pragma once

#include <rtp-client/rtp_client.hpp>
#include <track-negotiators/h264.hpp>
#include <track-negotiators/pcmu.hpp>
#include "rtcp_client.hpp"
#include <util/util.hpp>

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
    static RtspClientPtr create(const std::string& trackId, const std::string& url);
    RtspClient(const std::string& trackId, const std::string& url);
    ~RtspClient();

    bool start(std::function<void(CURLcode res, uint16_t statusCode)> cb);
    void stop();

    RtpClientPtr getVideoStream();
    RtpClientPtr getAudioStream();

    void setTrackNegotiators(TrackNegotiatorPtr videoNegotiator, TrackNegotiatorPtr audioNegotiator)
    {
        videoNegotiator_ = videoNegotiator;
        audioNegotiator_ = audioNegotiator;
    }


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
    void teardown();
    bool performSetupReq(const std::string& url, const std::string& transport);

    bool sendDescribe();

    bool parseSdpDescription(const std::string& desc);
    std::string parseControlAttribute(const std::string& att);

    static size_t writeFunc(void* ptr, size_t size, size_t nmemb, void* self);

    void resolveStart(CURLcode res, uint16_t statuscode);
    std::string toHex(uint8_t* data, size_t len);

    std::string trackId_;
    std::string url_;
    std::string username_;
    std::string password_;
    uint16_t port_ = 42222;
    bool stopped_ = false;

    std::function<void(CURLcode res, uint16_t statuscode)> startCb_;

    CurlAsyncPtr curl_;
    std::string curlHeaders_;
    std::string readBuffer_;
    std::string contentBase_;

    struct curl_slist* curlReqHeaders_ = NULL;

    std::string authHeader_;

    std::string sessionControlUrl_;

    RtpClientPtr videoStream_ = nullptr;
    TrackNegotiatorPtr videoNegotiator_;
    std::string videoControlUrl_;
    int videoPayloadType_;
    RtcpClientPtr videoRtcp_ = nullptr;

    RtpClientPtr audioStream_ = nullptr;
    TrackNegotiatorPtr audioNegotiator_;
    std::string audioControlUrl_;
    int audioPayloadType_;
    RtcpClientPtr audioRtcp_ = nullptr;

};


} // namespace
