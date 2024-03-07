#include "rtsp_client.hpp"

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
    : trackId_(trackId), url_(url)
{

}

RtspClient::~RtspClient()
{
    std::cout << "RtspClient destructor" << std::endl;
}

bool RtspClient::start(std::function<void(CURLcode res)> cb)
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
        if (res != CURLE_OK) {
            std::cout << "Failed to perform RTSP OPTIONS request: " << res << std::endl;
            return self->resolveStart(res);
        }
        std::cout << "Options request complete" << std::endl;
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

    // SENDING DESCRIBE REQ

    std::cout << "Sending RTSP DESCRIBE request" << std::endl;
    readBuffer_.clear();
    curlHeaders_.clear();

    res = curl_easy_setopt(curl, CURLOPT_HEADERDATA, &curlHeaders_);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl header data option" << std::endl;
        return resolveStart(res);
    }

    res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunc);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl write function option" << std::endl;
        return resolveStart(res);
    }

    res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&readBuffer_);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl write function option" << std::endl;
        return resolveStart(res);
    }

    res = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_DESCRIBE);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP request option" << std::endl;
        return resolveStart(res);
    }

    res = curl_->reinvoke();
    if (res != CURLE_OK) {
        std::cout << "Failed to perform RTSP DESCRIBE request" << std::endl;
        return resolveStart(res);
    }

    res = curl_easy_setopt(curl, CURLOPT_HEADERDATA, stdout);
    res = curl_easy_setopt(curl, CURLOPT_WRITEDATA, stdout);
    res = curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);

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
            return resolveStart(res);
        }
    }
    if (!audioControlUrl_.empty()) {
        std::cout << "Sending RTSP SETUP request for audio stream" << std::endl;
        std::stringstream trans;
        trans << "RTP/AVP;unicast;client_port=" << port_+2 << "-" << port_ + 3;
        std::string transStr = trans.str();
        if (!performSetupReq(audioControlUrl_, transStr)) {
            std::cout << "Failed to send Audio SETUP request" << std::endl;
            return resolveStart(res);
        }
    }
    std::cout << "RTSP setup completed successfully" << std::endl;

    if (!videoControlUrl_.empty()) {
        videoStream_ = RtpClient::create(trackId_ + "-video");
        videoStream_->setTrackNegotiator(videoCodec_);
        videoStream_->setPort(port_);

        videoRtcp_ = RtcpClient::create(port_ + 1);
        videoRtcp_->start();
    }

    if (!audioControlUrl_.empty()) {
        audioStream_ = RtpClient::create(trackId_ + "-audio");
        audioStream_->setTrackNegotiator(audioCodec_);
        audioStream_->setPort(port_ + 2);

        audioRtcp_ = RtcpClient::create(port_ + 3);
        audioRtcp_->start();
    }

    const char* range = "npt=now-";
    std::string uri = sessionControlUrl_;
    res = curl_easy_setopt(curl, CURLOPT_RTSP_STREAM_URI, uri.c_str());
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP stream URI option" << std::endl;
        return resolveStart(res);
    }
    res = curl_easy_setopt(curl, CURLOPT_RANGE, range);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Range option" << std::endl;
        return resolveStart(res);
    }
    res = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Request option" << std::endl;
        return resolveStart(res);
    }
    res = curl_->reinvoke();
    if (res != CURLE_OK) {
        std::cout << "Failed to perform RTSP PLAY request" << std::endl;
        return resolveStart(res);
    }

    // switch off using range again
    res = curl_easy_setopt(curl, CURLOPT_RANGE, NULL);
    if (res != CURLE_OK) {
        std::cout << "Failed to reset Curl RTSP Range option" << std::endl;
        return resolveStart(res);
    }

    return resolveStart(CURLE_OK);
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
                    videoPayloadType_ = videoCodec_->payloadType();
                }
            }
            else if (m->type() == "audio") {
                audioControlUrl_ = controlUrl;
                if (ptVec.size() > 0) {
                    audioPayloadType_ = ptVec[0];
                }
                else {
                    audioPayloadType_ = audioCodec_->payloadType();
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
    res = curl_easy_setopt(curl, CURLOPT_RTSP_TRANSPORT, transport.c_str());
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Transport option" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Request option" << std::endl;
        return false;
    }
    res = curl_->reinvoke();
    if (res != CURLE_OK) {
        std::cout << "Failed to perform RTSP SETUP request" << std::endl;
        return false;
    }
    return true;

}


void RtspClient::resolveStart(CURLcode res)
{
    // TODO: ensure everything is cleaned up/resolved
    if (startCb_) {
        auto cb = startCb_;
        startCb_ = nullptr;
        cb(res);
    }
}
} // namespace
