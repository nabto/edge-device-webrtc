#include "rtsp_client.hpp"

#include <jwt-cpp/jwt.h>

#include <openssl/md5.h>

#include <sstream>
#include <cstring>

namespace rtc {
using std::get;
using std::holds_alternative;
}  // namespace rtc

namespace nabto {

RtspClientPtr RtspClient::create(const std::string& trackId, const std::string& url)
{
    return std::make_shared<RtspClient>(trackId, url);

}

RtspClient::RtspClient(const std::string& trackId, const std::string& url)
    : trackId_(trackId)
{
    auto at = url.find("@");
    if (at == std::string::npos) {
        url_ = url;
    } else {
        auto col = url.find(":");
        col = url.find(":", col+1);
        if (col == std::string::npos) {
            std::cout << "Possibly invalid URL, contained `@` but not `:`: " << url << std::endl;
            url_ = url;
            return;
        }
        username_ = url.substr(7, col-7);
        password_ = url.substr(col+1, at-col-1);
        url_ = "rtsp://" + url.substr(at+1);
        std::cout << "Parsed URL     : " << url_ << std::endl;
        std::cout << "Parsed username: " << username_ << std::endl;
        std::cout << "Parsed password: " << password_ << std::endl;
    }

}

RtspClient::~RtspClient()
{
    std::cout << "RtspClient destructor" << std::endl;
    if (curlReqHeaders_ != NULL) {
        curl_slist_free_all(curlReqHeaders_);
    }
}

bool RtspClient::start(std::function<void(CURLcode res, uint16_t statuscode)> cb)
{

    curl_ = CurlAsync::create();
    if (curl_ == nullptr) {
        std::cout << "Failed to create CurlAsync object" << std::endl;
        return false;
    }
    startCb_ = cb;

    CURLcode res = CURLE_OK;
    CURL* curl = curl_->getCurl();

    // SENDING OPTIONS REQ
    std::cout << "Sending RTSP OPTIONS request" << std::endl;
    res = curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl URL option" << std::endl;
        return false;
    }

    res = curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, url_.c_str());
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP stream URI option" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Request options" << std::endl;
        return false;
    }

    auto self = shared_from_this();
    bool ok = curl_->asyncInvoke([self](CURLcode res, uint16_t statusCode) {
        if (res != CURLE_OK || statusCode > 299) {
            std::cout << "Failed to perform RTSP OPTIONS request: " << res << std::endl;
            return self->resolveStart(res, statusCode);
        }
        std::cout << "Options request complete " << res << " " << statusCode << std::endl;
        self->setupRtsp();
    });

    if (!ok) {
        return false;
    }

    return true;
}

void RtspClient::stop()
{
    if (videoRtcp_ != nullptr) {
        videoRtcp_->stop();
    }
    if (audioRtcp_ != nullptr) {
        audioRtcp_->stop();
    }
    teardown();
}

RtpClientPtr RtspClient::getVideoStream()
{
    return videoStream_;
}

RtpClientPtr RtspClient::getAudioStream()
{
    return audioStream_;
}



void RtspClient::setupRtsp() {
    // THIS IS CALLED FROM THE CURL WORKER THREAD!
    CURLcode res = CURLE_OK;
    CURL* curl = curl_->getCurl();

    if (!sendDescribe()) {
        std::cout << "sendDescribe() failed" << std::endl;
        return;
    }
    std::cout << "Read SDP description: " << std::endl << readBuffer_ << std::endl;

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
    } else {
        contentBase_ = url_;
    }
    std::cout << "Parsed Content-Base to: " << contentBase_ << std::endl;
    parseSdpDescription(readBuffer_);

    std::cout << "Parsed SDP description!" << std::endl << "  videoControlUrl: " << videoControlUrl_ << " video PT: " << videoPayloadType_ << std::endl;
    if(!audioControlUrl_.empty()) {
        std::cout << "  audioControlUrl: " << audioControlUrl_ << " audio PT: " << audioPayloadType_ << std::endl;
    } else {
        std::cout << "  no audio media found" << std::endl;
    }

    // SENDING SETUP REQ for video stream

    if (!videoControlUrl_.empty()) {
        std::cout << "Sending RTSP SETUP request for video stream" << std::endl;
        std::stringstream trans;
        trans << "RTP/AVP;unicast;client_port=" << port_ << "-" << port_+1;
        std::string transStr = trans.str();
        if (!performSetupReq(videoControlUrl_, transStr)) {
            std::cout << "Failed to send Video SETUP request" << std::endl;
            return resolveStart(res, 0);
        }
    }
    if (!audioControlUrl_.empty()) {
        std::cout << "Sending RTSP SETUP request for audio stream" << std::endl;
        std::stringstream trans;
        trans << "RTP/AVP;unicast;client_port=" << port_+2 << "-" << port_ + 3;
        std::string transStr = trans.str();
        if (!performSetupReq(audioControlUrl_, transStr)) {
            std::cout << "Failed to send Audio SETUP request" << std::endl;
            return resolveStart(res, 0);
        }
    }
    std::cout << "RTSP setup completed successfully" << std::endl;

    if (!videoControlUrl_.empty()) {
        videoStream_ = RtpClient::create(trackId_ + "-video");
        videoStream_->setTrackNegotiator(videoNegotiator_);
        videoStream_->setPort(port_);

        videoRtcp_ = RtcpClient::create(port_ + 1);
        videoRtcp_->start();
    }

    if (!audioControlUrl_.empty()) {
        audioStream_ = RtpClient::create(trackId_ + "-audio");
        audioStream_->setTrackNegotiator(audioNegotiator_);
        audioStream_->setPort(port_ + 2);

        audioRtcp_ = RtcpClient::create(port_ + 3);
        audioRtcp_->start();
    }

    const char* range = "npt=now-";
    std::string uri = sessionControlUrl_;
    res = curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri.c_str());
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP stream URI option" << std::endl;
        return resolveStart(res, 0);
    }
    res = curl_easy_setopt(curl, CURLOPT_RANGE, range);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Range option" << std::endl;
        return resolveStart(res, 0);
    }
    res = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Request option" << std::endl;
        return resolveStart(res, 0);
    }

    if (isDigestAuth_ && !setDigestHeader("PLAY", uri)) {
        std::cout << "Failed to set digest auth header" << std::endl;
        return resolveStart(CURLE_AUTH_ERROR, 0);
    }

    res = curl_->reinvoke();
    if (res != CURLE_OK) {
        std::cout << "Failed to perform RTSP PLAY request" << std::endl;
        return resolveStart(res, 0);
    }

    // switch off using range again
    res = curl_easy_setopt(curl, CURLOPT_RANGE, NULL);
    if (res != CURLE_OK) {
        std::cout << "Failed to reset Curl RTSP Range option" << std::endl;
        return resolveStart(res, 0);
    }

    return resolveStart(CURLE_OK, 0);
}

void RtspClient::teardown()
{
    // SENDING TEARDOWN REQ
    CURL* curl = curl_->getCurl();
    std::cout << "Sending RTSP TEARDOWN request" << std::endl;
    CURLcode res = curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, sessionControlUrl_.c_str());
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP stream URI option" << std::endl;
        return;
    }
    res = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_TEARDOWN);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Request options" << std::endl;
        return;
    }
    auto self = shared_from_this();
    curl_->asyncInvoke([self](CURLcode res, uint16_t statusCode) {
        if (res != CURLE_OK) {
            std::cout << "Failed to perform RTSP TEARDOWN request" << std::endl;
            return;
        }
        std::cout << "Teardown request complete" << std::endl;
    });
}


size_t RtspClient::writeFunc(void* ptr, size_t size, size_t nmemb, void* s)
{
    if (s == stdout) {
        std::cout << "s was stdout, this is header data" << std::endl;
        std::string data((char*)ptr, size * nmemb);
        std::cout << data << std::endl;
        return size * nmemb;
    }
    try {
        ((std::string*)s)->append((char*)ptr, size * nmemb);
    }
    catch (std::exception& ex) {
        std::cout << "WriteFunc failure" << std::endl;
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

    auto count = desc.mediaCount();
    for (size_t i = 0; i < count; i++) {
        if (rtc::holds_alternative<rtc::Description::Media*>(desc.media(i))) {
            auto m = rtc::get<rtc::Description::Media*>(desc.media(i));
            std::cout << "Found Media type: " << m->type() << std::endl;
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
            }
            else if (m->type() == "audio") {
                audioControlUrl_ = controlUrl;
                if (ptVec.size() > 0) {
                    audioPayloadType_ = ptVec[0];
                }
                else {
                    audioPayloadType_ = audioNegotiator_->payloadType();
                }
            }
            else {
                std::cout << "control attribute for unknown media type: " << m->type() << std::endl;
            }
        } else {
            std::cout << "media type not media" << std::endl;
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

bool RtspClient::performSetupReq(const std::string& url, const std::string& transport)
{
    // THIS IS CALLED FROM THE CURL WORKER THREAD
    CURLcode res = CURLE_OK;
    CURL* curl = curl_->getCurl();
    res = curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, url.c_str());
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP stream URI option" << std::endl;
        return false;
    }

    res = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Request option" << std::endl;
        return false;
    }

    if (isDigestAuth_) {
        std::cout << "indeed" << std::endl;
        if (!setDigestHeader("SETUP", url)) {
            std::cout << "Failed to set digest auth header" << std::endl;
            return false;
        }
    }

    res = curl_easy_setopt(curl, CURLOPT_RTSP_TRANSPORT, transport.c_str());
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Transport option" << std::endl;
        return false;
    }


    res = curl_->reinvoke();
    if (res != CURLE_OK) {
        std::cout << "Failed to perform RTSP SETUP request: " << res << std::endl;
        return false;
    }
    return true;

}

bool RtspClient::sendDescribe()
{
    // SENDING DESCRIBE REQ

    std::cout << "Sending RTSP DESCRIBE request" << std::endl;
    readBuffer_.clear();
    curlHeaders_.clear();
    CURL* curl = curl_->getCurl();

    CURLcode res = curl_easy_setopt(curl, CURLOPT_HEADERDATA, &curlHeaders_);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl header data option" << std::endl;
        resolveStart(res, 0);
        return false;
    }

    res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunc);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl write function option" << std::endl;
        resolveStart(res, 0);
        return false;
    }

    res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&readBuffer_);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl write function option" << std::endl;
        resolveStart(res, 0);
        return false;
    }

    res = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_DESCRIBE);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP request option" << std::endl;
        resolveStart(res, 0);
        return false;
    }

    uint16_t status = 0;
    curl_->reinvokeStatus(&res, &status);
    res = curl_easy_setopt(curl, CURLOPT_HEADERDATA, stdout);
    res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, stdout);
    res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);

    if (res != CURLE_OK) {
        std::cout << "Failed to perform RTSP DESCRIBE request" << std::endl;
        std::cout << "CurlHeaders: " << curlHeaders_ << std::endl;
        std::cout << "readBuffer: " << readBuffer_ << std::endl;
        resolveStart(res, status);
        return false;
    }

    if (status == 401 && authHeader_.empty()) {
        std::cout << "Got 401 doing auth describe" << std::endl;
        std::cout << "CurlHeaders: " << curlHeaders_ << std::endl;
        size_t pos = curlHeaders_.find("WWW-Authenticate: Basic");
        if (pos != std::string::npos) {
            std::string credStr = username_ + ":" + password_;
            std::cout << "Got Basic header login with: " << credStr << std::endl;
            // auto creds = jwt::base::trim<jwt::alphabet::base64>(jwt::base::encode<jwt::alphabet::base64>(credStr));
            auto creds = jwt::base::encode<jwt::alphabet::base64>(credStr);
            std::cout << "Creds: " << creds << std::endl;
            authHeader_ = "Authorization: Basic " + creds;
            curlReqHeaders_ = curl_slist_append(curlReqHeaders_, authHeader_.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlReqHeaders_);

            return sendDescribe();
        }

        pos = curlHeaders_.find("WWW-Authenticate: Digest");
        if (pos != std::string::npos) {
            std::cout << "Got digest header" << std::endl;
            isDigestAuth_ = true;

            auto realmPos = curlHeaders_.find("realm=\"");
            if (realmPos == std::string::npos) {
                std::cout << "Could not find realm string" << std::endl;
                return false;
            }
            realmPos += strlen("realm=\"");
            auto realm = curlHeaders_.substr(realmPos);
            realmPos = realm.find("\"");
            if (realmPos == std::string::npos) {
                std::cout << "Could not find realm trailing \"" << std::endl;
                return false;
            }
            realm_ = realm.substr(0, realmPos);
            std::cout << "Found Realm: " << realm_ << std::endl;

            auto noncePos = curlHeaders_.find("nonce=\"");
            if (noncePos == std::string::npos) {
                std::cout << "Could not find nonce string" << std::endl;
                return false;
            }
            noncePos += strlen("nonce=\"");
            auto nonce = curlHeaders_.substr(noncePos);
            noncePos = nonce.find("\"");
            if (noncePos == std::string::npos) {
                std::cout << "Could not find nonce trailing \"" << std::endl;
                return false;
            }
            nonce_ = nonce.substr(0, noncePos);
            std::cout << "Found nonce: " << nonce_ << std::endl;

            // HA1
            std::string str1 = username_ + ":" + realm_ +":" + password_;
            unsigned char ha1[MD5_DIGEST_LENGTH];
            MD5((unsigned char*)str1.c_str(), str1.size(), ha1);
            ha1_ = toHex(ha1, MD5_DIGEST_LENGTH);
            std::cout << "HA1: " << ha1_ << std::endl;

            setDigestHeader("DESCRIBE", "rtsp://127.0.0.1:8554/video");
            return sendDescribe();
        }

        std::cout << "WWW-Authenticate header missing from 401 response" << std::endl;

        return false;
    } else if (status > 299) {
        return false;
    }


    return true;
}

bool RtspClient::setDigestHeader(std::string method, std::string url)
{
    // HA2
    std::string str2 = method + ":" + url;
    std::cout << "str2: " << str2 << std::endl;
    unsigned char ha2[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)str2.c_str(), str2.size(), ha2);
    std::cout << "HA2: " << toHex(ha2, MD5_DIGEST_LENGTH) << std::endl;

    // RESPONSE
    std::string strResp = ha1_ + ":" + nonce_ + ":" + toHex(ha2, MD5_DIGEST_LENGTH);

    unsigned char response[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)strResp.c_str(), strResp.size(), response);

    std::cout << "response: " << toHex(response, MD5_DIGEST_LENGTH) << std::endl;

    authHeader_ = "Authorization: Digest username=\"" + username_ +
        "\", realm=\"" + realm_ +
        "\", nonce=\"" + nonce_ +
        "\", uri=\"" + url +
        "\", response=\"" + toHex(response, MD5_DIGEST_LENGTH) + "\"";

    if (curlReqHeaders_ != NULL) {
        curl_slist_free_all(curlReqHeaders_);
        curlReqHeaders_ = NULL;
    }
    curlReqHeaders_ = curl_slist_append(curlReqHeaders_, authHeader_.c_str());
    CURL* curl = curl_->getCurl();
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curlReqHeaders_);

    return true;
}

void RtspClient::resolveStart(CURLcode res, uint16_t statuscode)
{
    // TODO: ensure everything is cleaned up/resolved
    if (startCb_) {
        auto cb = startCb_;
        startCb_ = nullptr;
        cb(res, statuscode);
    }
}

std::string RtspClient::toHex(uint8_t* data, size_t len)
{
    std::stringstream stream;
    for (size_t i = 0; i < len; i++) {
        stream << std::setfill('0') << std::setw(2) << std::hex << (int)data[i];
    }
    std::string result(stream.str());
    return result;
}

} // namespace
