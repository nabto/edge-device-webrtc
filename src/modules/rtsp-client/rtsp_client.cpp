#include "rtsp_client.hpp"

#ifdef NABTO_RTSP_HAS_BASIC_AUTH
#include <jwt-cpp/jwt.h>
#endif

#ifdef NABTO_RTSP_HAS_DIGEST_AUTH
#include <openssl/md5.h>
#endif

#include <sstream>
#include <cstring>

namespace rtc {
using std::get;
using std::holds_alternative;
}  // namespace rtc

namespace nabto {

class MockMediaTrack : public MediaTrack {
public:
    static MediaTrackPtr create(const rtc::Description::Media& media)
    {
        return std::make_shared<MockMediaTrack>(media);
    }

    MockMediaTrack(const rtc::Description::Media& media): media_(media), MediaTrack("", media.generateSdp()) { }
    std::string getTrackId() { return ""; }
    std::string getSdp() { return media_.generateSdp(); }
    void setSdp(const std::string& sdp) { return; }
    bool send(const uint8_t* buffer, size_t length) {return false;}
    void setReceiveCallback(MediaRecvCallback cb) {};
    void setCloseCallback(std::function<void()> cb) {};
    void setErrorState(enum ErrorState state){};
private:
    rtc::Description::Media media_;
};

std::string toHex(uint8_t* data, size_t len)
{
    std::stringstream stream;
    for (size_t i = 0; i < len; i++) {
        stream << std::setfill('0') << std::setw(2) << std::hex << (int)data[i];
    }
    std::string result(stream.str());
    return result;
}



#ifdef NABTO_RTSP_HAS_DIGEST_AUTH
std::string md5(std::string& message) {
    EVP_MD_CTX* mdctx;
    unsigned char md_value[MD5_DIGEST_LENGTH];
    const EVP_MD* md = EVP_get_digestbyname("MD5");
    unsigned int md_len;

    if (md == NULL) {
        NPLOGE << "Failed to get MD5 digest";
        return "";
    }

    mdctx = EVP_MD_CTX_new();
    if (!EVP_DigestInit_ex2(mdctx, md, NULL)) {
        NPLOGE << "Message digest initialization failed.";;
        EVP_MD_CTX_free(mdctx);
        return "";
    }
    if (!EVP_DigestUpdate(mdctx, message.c_str(), message.size())) {
        NPLOGE << "Message digest update failed.";;
        EVP_MD_CTX_free(mdctx);
        return "";
    }
    if (!EVP_DigestFinal_ex(mdctx, md_value, &md_len)) {
        NPLOGE << "Message digest finalization failed.";;
        EVP_MD_CTX_free(mdctx);
        return "";
    }
    EVP_MD_CTX_free(mdctx);
    return toHex(md_value, MD5_DIGEST_LENGTH);
}
#endif

RtspClientPtr RtspClient::create(const RtspClientConf& conf)
{
    return std::make_shared<RtspClient>(conf);

}

RtspClient::RtspClient(const RtspClientConf& conf)
    : trackId_(conf.trackId)
{
    auto url = conf.url;
    auto at = url.find("@");
    if (at == std::string::npos) {
        url_ = url;
    } else {
        auto col = url.find(":");
        col = url.find(":", col+1);
        if (col == std::string::npos) {
            NPLOGE << "Possibly invalid URL, contained `@` but not `:`: " << url;
            url_ = url;
            return;
        }
        username_ = url.substr(7, col-7);
        username_ = urlDecode(username_);
        password_ = url.substr(col+1, at-col-1);
        password_ = urlDecode(password_);
        url_ = "rtsp://" + url.substr(at+1);
        NPLOGI << "Parsed URL     : " << url_;
        NPLOGI << "Parsed username: " << username_;
        NPLOGI << "Parsed password: " << password_;
    }

    preferTcp_ = conf.preferTcp;
    port_ = conf.port;

    videoNegotiator_ = conf.videoNegotiator;
    if (conf.videoRepack != nullptr) {
        videoRepack_ = conf.videoRepack;
    }

    audioNegotiator_ = conf.audioNegotiator;
    if (conf.audioRepack != nullptr) {
        audioRepack_ = conf.audioRepack;
    }

}

RtspClient::~RtspClient()
{
    if (curlReqHeaders_ != NULL) {
        curl_slist_free_all(curlReqHeaders_);
    }
}

void RtspClient::stop()
{
    if (videoRtcp_ != nullptr) {
        videoRtcp_->stop();
    }
    if (audioRtcp_ != nullptr) {
        audioRtcp_->stop();
    }
    if (tcpClient_ != nullptr) {
        tcpClient_->stop();
    }
    curl_->stop();
    // teardown();
}

bool RtspClient::close(std::function<void()> cb)
{
    return teardown(cb);
}

void RtspClient::addConnection(NabtoDeviceConnectionRef ref, MediaTrackPtr videoTrack, MediaTrackPtr audioTrack)
{
    if (tcpClient_ != nullptr) {
        tcpClient_->setConnection(ref, videoTrack, audioTrack);
    }
    if (videoStream_ != nullptr && videoTrack != nullptr) {
        videoStream_->addConnection(ref, videoTrack);
    }
    if (audioStream_ != nullptr && audioTrack != nullptr) {
        audioStream_->addConnection(ref, audioTrack);
    }
}

void RtspClient::removeConnection(NabtoDeviceConnectionRef ref)
{
    if (videoStream_ != nullptr) {
        videoStream_->removeConnection(ref);
    }
    if (audioStream_ != nullptr) {
        audioStream_->removeConnection(ref);
    }
}

bool RtspClient::start(std::function<void(std::optional<std::string> error)> cb)
{

    curl_ = CurlAsync::create();
    if (curl_ == nullptr) {
        NPLOGE << "Failed to create CurlAsync object";
        return false;
    }
    startCb_ = cb;

    CURLcode res = CURLE_OK;
    CURL* curl = curl_->getCurl();

    // SENDING OPTIONS REQ
    NPLOGD << "Sending RTSP OPTIONS request";

    if ((res = curl_easy_setopt(curl, CURLOPT_URL, url_.c_str())) != CURLE_OK ||
        (res = curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, url_.c_str())) != CURLE_OK ||
        (res = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS)) != CURLE_OK
    ) {
        NPLOGE << "Failed to initialize Curl OPTIONS request with CURLE: " << curl_easy_strerror(res);
        return false;
    }

    auto self = shared_from_this();
    return curl_->asyncInvoke([self](CURLcode res, uint16_t statusCode) {
        if (res != CURLE_OK || statusCode > 299) {
            NPLOGE << "Failed to perform RTSP OPTIONS request: " << curl_easy_strerror(res);
            std::string msg = res != CURLE_OK ? "Failed to send Options Request" : ("RTSP OPTIONS request failed with status code: " + statusCode);
            return self->resolveStart(msg);
        }
        NPLOGD << "Options request complete " << curl_easy_strerror(res) << " " << statusCode;
        self->setupRtsp();
    });
}

void RtspClient::setupRtsp() {
    // THIS IS CALLED FROM THE CURL WORKER THREAD!
    CURLcode res = CURLE_OK;
    CURL* curl = curl_->getCurl();

    // DESCRIBE REQ
    auto ret = sendDescribe();
    if (ret.has_value()) {
        return resolveStart(ret);
    }
    NPLOGD << "Read SDP description: " << readBuffer_;

    if (!parseDescribeHeaders() ||
        !parseSdpDescription(readBuffer_)) {
       return resolveStart("Failed to parse DESCRIBE response");
    }

    NPLOGD << "Parsed SDP description!" << std::endl << "  videoControlUrl: " << videoControlUrl_ << " video PT: " << videoPayloadType_;
    if(!audioControlUrl_.empty()) {
        NPLOGD << "  audioControlUrl: " << audioControlUrl_ << " audio PT: " << audioPayloadType_;
    } else {
        NPLOGD << "  no audio media found";
    }

    if (videoControlUrl_.empty() && audioControlUrl_.empty()) {
        return resolveStart("Describe response contained no feeds");
    }

    // SENDING SETUP REQ for video stream
    if (!videoControlUrl_.empty()) {
        NPLOGD << "Sending RTSP SETUP request for video stream";
        std::string transStr = "RTP/AVP;unicast;client_port=" + std::to_string(port_) + "-" + std::to_string(port_ + 1);
        if (preferTcp_) {
            transStr = "RTP/AVP/TCP;unicast;interleaved=0-1";
        }
        auto r = performSetupReq(videoControlUrl_, transStr);
        if (r.has_value()) {
            return resolveStart(r);
        }

        if (preferTcp_) {
            if (tcpClient_ == nullptr) {
                TcpRtpClientConf conf = { curl_, sessionControlUrl_, videoNegotiator_, audioNegotiator_, videoRepack_, audioRepack_ };
                tcpClient_ = TcpRtpClient::create(conf);
            }
        } else {
            nabto::RtpClientConf conf = { trackId_ + "-video", std::string(), port_, videoNegotiator_, videoRepack_ };
            videoStream_ = RtpClient::create(conf);

            videoRtcp_ = RtcpClient::create(port_ + 1);
            videoRtcp_->start();
        }
    }
    if (!audioControlUrl_.empty()) {
        NPLOGD << "Sending RTSP SETUP request for audio stream";
        std::string transStr = "RTP/AVP;unicast;client_port=" + std::to_string(port_+2) + "-" + std::to_string(port_ + 3);
        if (preferTcp_) {
            transStr = "RTP/AVP/TCP;unicast;interleaved=2-3";
        }
        auto r = performSetupReq(audioControlUrl_, transStr);
        if (r.has_value()) {
            return resolveStart(r);
        }

        if (preferTcp_) {
            if (tcpClient_ == nullptr) {
                TcpRtpClientConf conf = { curl_, sessionControlUrl_, videoNegotiator_, audioNegotiator_, videoRepack_, audioRepack_ };
                tcpClient_ = TcpRtpClient::create(conf);
            }
        } else {
            nabto::RtpClientConf conf = { trackId_ + "-audio", std::string(), (uint16_t)(port_ + 2), audioNegotiator_, audioRepack_ };
            audioStream_ = RtpClient::create(conf);

            audioRtcp_ = RtcpClient::create(port_ + 3);
            audioRtcp_->start();
        }
    }

    const char* range = "npt=now-";

    if ((res = curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, sessionControlUrl_.c_str())) != CURLE_OK ||
        (res = curl_easy_setopt(curl, CURLOPT_RANGE, range)) != CURLE_OK ||
        (res = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY)) != CURLE_OK
    ) {
        NPLOGE << "Failed to create PLAY request with: " << curl_easy_strerror(res);
        resolveStart("Failed to create PLAY request");
    }

    if (isDigestAuth_ && !setDigestHeader("PLAY", sessionControlUrl_)) {
        NPLOGE << "Failed to set digest auth header";
        return resolveStart("Failed to set Authorization Digest header");
    }

    uint16_t status = 0;
    curl_->reinvokeStatus(&res, &status);
    if (res != CURLE_OK || status > 299) {
        NPLOGE << "Failed to perform RTSP PLAY request with: " << curl_easy_strerror(res);
        std::string msg = res != CURLE_OK ? "Failed to send PLAY Request" : ("RTSP PLAY request failed with status code: " + status);
        return resolveStart(msg);
    }

    // switch off using range again
    res = curl_easy_setopt(curl, CURLOPT_RANGE, NULL);
    if (res != CURLE_OK) {
        NPLOGE << "Failed to reset Curl RTSP Range option with: " << curl_easy_strerror(res);
        return resolveStart("Failed to reset Curl RTSP range option");
    }

    resolveStart();

    if (tcpClient_ != nullptr) {
        tcpClient_->run();
    }
}

bool RtspClient::teardown(std::function<void()> cb)
{
    // SENDING TEARDOWN REQ
    CURL* curl = curl_->getCurl();
    CURLcode res = CURLE_OK;
    NPLOGD << "Sending RTSP TEARDOWN request";

    if ((res = curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, sessionControlUrl_.c_str())) != CURLE_OK ||
        (res = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_TEARDOWN)) != CURLE_OK) {
        NPLOGE << "Failed to create RTSP Teardown request";
        return false;
    }

    auto self = shared_from_this();
    return curl_->asyncInvoke([self, cb](CURLcode res, uint16_t statusCode) {
        if (res != CURLE_OK) {
            NPLOGE << "Failed to perform RTSP TEARDOWN request with: " << curl_easy_strerror(res);
        } else {
            NPLOGD << "Teardown request complete";
        }
        cb();
    });
}


size_t RtspClient::writeFunc(void* ptr, size_t size, size_t nmemb, void* s)
{
    if (s == stdout) {
        NPLOGD << "s was stdout, this is header data";
        std::string data((char*)ptr, size * nmemb);
        NPLOGD << data;
        return size * nmemb;
    }
    try {
        ((std::string*)s)->append((char*)ptr, size * nmemb);
    }
    catch (std::exception& ex) {
        NPLOGE << "WriteFunc failure";
        return size * nmemb;
    }
    return size * nmemb;
}

bool RtspClient::parseSdpDescription(const std::string& sdp)
{

    auto desc = rtc::Description(sdp);
    auto atts = desc.attributes();

    for (auto a : atts) {
        if (a.rfind("control:",0) == 0) {
            sessionControlUrl_ = parseControlAttribute(a);
        }
    }
    if (sessionControlUrl_.empty()) {
        sessionControlUrl_ = contentBase_;
    }

    auto count = desc.mediaCount();
    for (size_t i = 0; i < count; i++) {
        if (rtc::holds_alternative<rtc::Description::Media*>(desc.media(i))) {
            auto m = rtc::get<rtc::Description::Media*>(desc.media(i));
            NPLOGD << "Found Media type: " << m->type();
            std::string controlUrl;

            auto mAtts = m->attributes();
            for (auto a : mAtts) {
                if (a.rfind("control:", 0) == 0) {
                    controlUrl = parseControlAttribute(a);
                }
            }

            auto ptVec = m->payloadTypes();

            if (m->type() == "video") {
                videoControlUrl_ = controlUrl;
                if (ptVec.size() > 0) {
                    videoPayloadType_ = ptVec[0];
                } else {
                    videoPayloadType_ = videoNegotiator_->payloadType();
                }
                auto mediaMock = MockMediaTrack::create(*m);
                if (videoNegotiator_->match(mediaMock) == 0) {
                    NPLOGE << "RTSP server offered invalid video codec. The video feed likely won't work!";
                }
            }
            else if (m->type() == "audio") {
                audioControlUrl_ = controlUrl;
                if (ptVec.size() > 0) {
                    audioPayloadType_ = ptVec[0];
                }
                else {
                    audioPayloadType_ = audioNegotiator_->payloadType();
                }
                auto mediaMock = MockMediaTrack::create(*m);
                if (audioNegotiator_->match(mediaMock) == 0) {
                    NPLOGE << "RTSP server offered invalid audio codec. The audio feed likely won't work!";
                }
            }
            else {
                NPLOGE << "control attribute for unknown media type: " << m->type();
            }
        } else {
            NPLOGD << "media type not media";
        }
    }
    return true;
}

std::string RtspClient::parseControlAttribute(const std::string& att)
{
    auto url = att.substr(strlen("control:"));
    if (url.empty() || url[0] == '*') {
        // if * use Content-Base
        return contentBase_;
    }
    else if (url.rfind("rtsp://", 0) == 0) {
        // is full url as it starts with rtsp://
        return url;
    }
    else {
        // is relative URL
        return contentBase_ + url;
    }

}

std::optional<std::string> RtspClient::performSetupReq(const std::string& url, const std::string& transport)
{
    // THIS IS CALLED FROM THE CURL WORKER THREAD
    CURLcode res = CURLE_OK;
    CURL* curl = curl_->getCurl();

    if ((res = curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, url.c_str())) != CURLE_OK ||
        (res = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP)) != CURLE_OK) {
        NPLOGE << "Failed to create SETUP request with error: " << curl_easy_strerror(res);
        return "Failed to create SETUP request";
    }

    if (isDigestAuth_) {
        if (!setDigestHeader("SETUP", url)) {
            NPLOGE << "Failed to set digest auth header";
            return "Failed to set authorization header";
        }
    }

    res = curl_easy_setopt(curl, CURLOPT_RTSP_TRANSPORT, transport.c_str());
    if (res != CURLE_OK) {
        NPLOGE << "Failed to set Curl RTSP Transport option: " << curl_easy_strerror(res);
        return "Failed to set Transport header";
    }


    res = curl_->reinvoke();
    if (res != CURLE_OK) {
        NPLOGE << "Failed to perform RTSP SETUP request: " << curl_easy_strerror(res);
        return "Failed to perform RTSP SETUP request";
    }
    return std::nullopt;

}

std::optional<std::string> RtspClient::sendDescribe()
{
    // SENDING DESCRIBE REQ

    NPLOGD << "Sending RTSP DESCRIBE request";
    readBuffer_.clear();
    curlHeaders_.clear();
    CURL* curl = curl_->getCurl();
    CURLcode res = CURLE_OK;


    if ((res = curl_easy_setopt(curl, CURLOPT_HEADERDATA, &curlHeaders_)) != CURLE_OK ||
        (res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunc)) != CURLE_OK ||
        (res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&readBuffer_)) != CURLE_OK ||
        (res = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_DESCRIBE)) != CURLE_OK
    ) {
        NPLOGE << "Failed to create RTSP Describe request. CURLcode: " << curl_easy_strerror(res);
        return "Failed to create RTSP Describe request. CURLcode: " + std::to_string(res);
    }

    uint16_t status = 0;
    curl_->reinvokeStatus(&res, &status);

    if ((res = curl_easy_setopt(curl, CURLOPT_HEADERDATA, stdout)) != CURLE_OK ||
        (res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, stdout)) != CURLE_OK ||
        (res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL)) != CURLE_OK) {
        NPLOGE << "Failed to perform RTSP Describe request. CURLcode: " << curl_easy_strerror(res);
        return "Failed to perform RTSP DESCRIBE request. CURLcode: " + std::to_string(res);
    }

    if (status == 401 && authHeader_.empty()) {
        NPLOGD << "Got 401 trying to authenticate";
        size_t pos = curlHeaders_.find("WWW-Authenticate: Basic");
        if (pos != std::string::npos) {
#ifdef NABTO_RTSP_HAS_BASIC_AUTH
            std::string credStr = username_ + ":" + password_;
            NPLOGD << "Trying Basic auth with: " << credStr;
            auto creds = jwt::base::encode<jwt::alphabet::base64>(credStr);
            authHeader_ = "Authorization: Basic " + creds;
            curlReqHeaders_ = curl_slist_append(curlReqHeaders_, authHeader_.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlReqHeaders_);
            return sendDescribe();
#else
            NPLOGE << "Server requires Basic Auth, but Basic Auth is disabled";
            return "Unsupported basic auth required";
#endif
        }

        pos = curlHeaders_.find("WWW-Authenticate: Digest");
        if (pos != std::string::npos) {
#ifdef NABTO_RTSP_HAS_DIGEST_AUTH
            NPLOGD << "Trying Digest auth";
            isDigestAuth_ = true;

            auto realmPos = curlHeaders_.find("realm=\"");
            if (realmPos == std::string::npos) {
                NPLOGE << "Could not find realm string";
                return "Invalid Describe response header";
            }
            realmPos += strlen("realm=\"");
            auto realm = curlHeaders_.substr(realmPos);
            realmPos = realm.find("\"");
            if (realmPos == std::string::npos) {
                NPLOGE << "Could not find realm trailing \"";
                return "Invalid Describe response header";
            }
            realm_ = realm.substr(0, realmPos);
            NPLOGD << "Found Realm: " << realm_;

            auto noncePos = curlHeaders_.find("nonce=\"");
            if (noncePos == std::string::npos) {
                NPLOGE << "Could not find nonce string";
                return "Invalid Describe response header";
            }
            noncePos += strlen("nonce=\"");
            auto nonce = curlHeaders_.substr(noncePos);
            noncePos = nonce.find("\"");
            if (noncePos == std::string::npos) {
                NPLOGE << "Could not find nonce trailing \"";
                return "Invalid Describe response header";
            }
            nonce_ = nonce.substr(0, noncePos);
            NPLOGD << "Found nonce: " << nonce_;

            // HA1
            std::string str1 = username_ + ":" + realm_ +":" + password_;
            ha1_ = md5(str1);
            NPLOGD << "HA1: " << ha1_;

            setDigestHeader("DESCRIBE", "rtsp://127.0.0.1:8554/video");
            return sendDescribe();
#else
            NPLOGE << "Server requires Digest Auth, but Digest Auth is disabled";
            return "Unsupported digest auth required";
#endif
        }

        NPLOGE << "WWW-Authenticate header missing from 401 response";

        return "Invalid Describe response header";
    } else if (status > 299) {
        return "Describe request failed with status code: " + std::to_string(status);
    }

    return std::nullopt;
}

bool RtspClient::setDigestHeader(std::string method, std::string url)
{
#ifdef NABTO_RTSP_HAS_DIGEST_AUTH
    // HA2
    std::string str2 = method + ":" + url;
    NPLOGD << "str2: " << str2;

    std::string ha2Hex = md5(str2);
    if (ha2Hex.empty()) {
        NPLOGE << "Failed to create MD5 hash";
        return false;
    }
    NPLOGD << "HA2: " << ha2Hex;

    // RESPONSE
    std::string strResp = ha1_ + ":" + nonce_ + ":" + ha2Hex;

    std::string response = md5(strResp);

    NPLOGD << "response: " << response;

    authHeader_ = "Authorization: Digest username=\"" + username_ +
        "\", realm=\"" + realm_ +
        "\", nonce=\"" + nonce_ +
        "\", uri=\"" + url +
        "\", response=\"" + response + "\"";

    if (curlReqHeaders_ != NULL) {
        curl_slist_free_all(curlReqHeaders_);
        curlReqHeaders_ = NULL;
    }
    curlReqHeaders_ = curl_slist_append(curlReqHeaders_, authHeader_.c_str());
    CURL* curl = curl_->getCurl();
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlReqHeaders_);

    return true;
#else
    return true;
#endif
}

void RtspClient::resolveStart(std::optional<std::string> error)
{
    if (startCb_) {
        auto cb = startCb_;
        startCb_ = nullptr;
        cb(error);
    }
}

bool RtspClient::parseDescribeHeaders()
{
    // RFC2326 C.1:
    // ... look for a base URL in the following order:
    // 1.     The RTSP Content-Base field
    // 2.     The RTSP Content-Location field
    // 3.     The RTSP request URL
    size_t pos = curlHeaders_.find("Content-Base: ");
    if (pos != std::string::npos) {
        contentBase_ = curlHeaders_.substr(pos + strlen("Content-Base: "));
        if ((pos = contentBase_.find("\r\n")) != std::string::npos) {
            contentBase_ = contentBase_.substr(0, pos);
        }
    }
    else if ((pos = curlHeaders_.find("Content-Location: ")) != std::string::npos) {
        contentBase_ = curlHeaders_.substr(pos);
        if ((pos = contentBase_.find("\r\n")) != std::string::npos) {
            contentBase_ = contentBase_.substr(0, pos);
        }
    }
    else {
        contentBase_ = url_;
    }
    NPLOGD << "Parsed Content-Base to: " << contentBase_;
    return true;
}


} // namespace
