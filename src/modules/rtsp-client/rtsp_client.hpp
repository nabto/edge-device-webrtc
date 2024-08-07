#pragma once

#include <rtp-client/rtp_client.hpp>
#include <track-negotiators/h264.hpp>
#include <track-negotiators/pcmu.hpp>
#include <rtp-repacketizer/rtp_repacketizer.hpp>
#include "rtcp_client.hpp"
#include "tcp_rtp_client.hpp"
#include <util/util.hpp>

#include <rtc/rtc.hpp>

#include <curl/curl.h>
#include <sys/socket.h>
typedef int SOCKET;

#include <memory>
#include <thread>

namespace nabto {

const int RTSP_BUFFER_SIZE = 2048;

class RtspClient;

typedef std::shared_ptr<RtspClient> RtspClientPtr;

class RtspClientConf {
public:
    std::string trackId;
    std::string url;
    TrackNegotiatorPtr videoNegotiator;
    TrackNegotiatorPtr audioNegotiator;
    RtpRepacketizerFactoryPtr videoRepack;
    RtpRepacketizerFactoryPtr audioRepack;
    bool preferTcp = true;
    // Sets which transport ports to make RTSP server send RTP traffic to.
    // 4 ports are used in total:
    //   port  : Port for video stream
    //   port+1: Port for video RTCP
    //   port+2: Port for Audio stream if exists
    //   port+3: Port for Audio RTCP if exists
    // if unset port defaults to 42222 meaning 42222-42225 is used.
    uint16_t port = 42222;
};

class RtspClient : public std::enable_shared_from_this<RtspClient>
{
public:
    static RtspClientPtr create(const RtspClientConf& conf);
    RtspClient(const RtspClientConf& conf);
    ~RtspClient();

    bool start(std::function<void(std::optional<std::string> error)> cb);
    bool close(std::function<void()> cb);
    void stop();

    void addConnection(NabtoDeviceConnectionRef ref, MediaTrackPtr videoTrack, MediaTrackPtr audioTrack);
    void removeConnection(NabtoDeviceConnectionRef ref);

private:
    void setupRtsp();
    bool teardown(std::function<void()> cb);

    std::optional<std::string> sendDescribe();
    bool parseDescribeHeaders();



    std::optional<std::string> performSetupReq(const std::string& url, const std::string& transport);
    bool parseSdpDescription(const std::string& desc);
    std::string parseControlAttribute(const std::string& att);

    static size_t writeFunc(void* ptr, size_t size, size_t nmemb, void* self);

    void resolveStart(std::optional<std::string> error = std::nullopt);

    bool setDigestHeader(std::string method, std::string url);

    std::string trackId_;
    std::string url_;
    uint16_t port_ = 42222;
    bool stopped_ = false;
    bool preferTcp_ = true;

    std::function<void(std::optional<std::string> error)> startCb_;

    CurlAsyncPtr curl_;
    std::string curlHeaders_;
    std::string readBuffer_;
    std::string contentBase_;

    struct curl_slist* curlReqHeaders_ = NULL;
    bool isDigestAuth_ = false;
    std::string username_;
    std::string password_;
    std::string ha1_;
    std::string nonce_;
    std::string realm_;

    std::string authHeader_;


    std::string sessionControlUrl_;

    TcpRtpClientPtr tcpClient_ = nullptr;

    RtpClientPtr videoStream_ = nullptr;
    TrackNegotiatorPtr videoNegotiator_;
    std::string videoControlUrl_;
    int videoPayloadType_;
    RtcpClientPtr videoRtcp_ = nullptr;
    RtpRepacketizerFactoryPtr videoRepack_ = RtpRepacketizerFactory::create();

    RtpClientPtr audioStream_ = nullptr;
    TrackNegotiatorPtr audioNegotiator_;
    std::string audioControlUrl_;
    int audioPayloadType_;
    RtcpClientPtr audioRtcp_ = nullptr;
    RtpRepacketizerFactoryPtr audioRepack_ = RtpRepacketizerFactory::create();

};


} // namespace
