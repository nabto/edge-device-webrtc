#include "rtsp_client.hpp"

#include <sstream>
#include <cstring>

namespace nabto {

RtspClientPtr RtspClient::create(std::string trackId, std::string& url)
{
    return std::make_shared<RtspClient>(trackId, url);

}

RtspClient::RtspClient(std::string& trackId, std::string& url)
    : trackId_(trackId), url_(url)
{

}

RtspClient::~RtspClient()
{
    std::cout << "RtspClient destructor" << std::endl;
}

void RtspClient::start()
{
    if (!setupCurl()) {
        std::cout << "setupCurl() failed" << std::endl;
        return;
    }
    if (!setupRtsp()) {
        std::cout << "setupRtsp() failed" << std::endl;
        return;
    }
    // TODO: create RtpClient for audio as well
    videoStream_ = RtpClient::create(trackId_ + "-video");
    videoStream_->setRtpCodecMatcher(&videoCodec_);
    videoStream_->setVideoPort(port_);

    rtspPlay();
}

void RtspClient::stop()
{

}

MediaStreamPtr RtspClient::getVideoStream()
{
    return videoStream_;
}

MediaStreamPtr RtspClient::getAudioStream()
{
    return audioStream_;
}


bool RtspClient::setupCurl() {
    CURLcode res;
    res = curl_global_init(CURL_GLOBAL_ALL);
    if (res != CURLE_OK) {
        std::cout << "Failed to initialize Curl global" << std::endl;
        return false;
    }
    curl_ = curl_easy_init();
    if (!curl_) {
        std::cout << "Failed to initialize Curl easy" << std::endl;
        return false;
    }

    res = curl_easy_setopt(curl_, CURLOPT_VERBOSE, 0L);
    if (res != CURLE_OK) {
        std::cout << "Failed to set curl logging option" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 1L);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl progress option" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_HEADERDATA, stdout);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl header data option" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str());
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl URL option" << std::endl;
        return false;
    }
    return true;
}

bool RtspClient::setupRtsp() {
    CURLcode res = CURLE_OK;

    // SENDING OPTIONS REQ
    std::cout << "Sending RTSP OPTIONS request" << std::endl;
    res = curl_easy_setopt(curl_, CURLOPT_RTSP_STREAM_URI, url_.c_str());
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP stream URI option" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Request options" << std::endl;
        return false;
    }
    res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        std::cout << "Failed to perform RTSP OPTIONS request" << std::endl;
        return false;
    }

    std::cout << "Options request complete" << std::endl;

    // SENDING DESCRIBE REQ

    std::cout << "Sending RTSP DESCRIBE request" << std::endl;
    // memset(rtspDescription_, 0, 1024);
    std::string readBuffer;
    curlHeaders_.clear();

    res = curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &curlHeaders_);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl header data option" << std::endl;
        return false;
    }

    res = curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeFunc);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl write function option" << std::endl;
        return false;
    }

    res = curl_easy_setopt(curl_, CURLOPT_WRITEDATA, (void*)&readBuffer);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl write function option" << std::endl;
        return false;
    }

    res = curl_easy_setopt(curl_, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_DESCRIBE);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP request option" << std::endl;
        return false;
    }

    res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        std::cout << "Failed to perform RTSP DESCRIBE request" << std::endl;
        return false;
    }

    res = curl_easy_setopt(curl_, CURLOPT_HEADERDATA, stdout);
    res = curl_easy_setopt(curl_, CURLOPT_WRITEDATA, stdout);
    res = curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, NULL);

    std::cout << "Read SDP description: " << std::endl << readBuffer << std::endl;

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
    parseSdpDescription(readBuffer);

    std::cout << "Parsed SDP description!" << std::endl << "  videoControlUrl: " << videoControlUrl_ << " video PT: " << videoPayloadType_ << std::endl;
    if(!audioControlUrl_.empty()) {
        std::cout << "  audioControlUrl: " << audioControlUrl_ << " audio PT: " << audioPayloadType_ << std::endl;
    } else {
        std::cout << "  no audio media found" << std::endl;
    }

    // SENDING SETUP REQ for video stream

    // TODO: maybe ephemeral ports
    if (!videoControlUrl_.empty()) {
        std::cout << "Sending RTSP SETUP request for video stream" << std::endl;
        std::stringstream trans;
        trans << "RTP/AVP;unicast;client_port=" << port_ << "-" << port_+1;
        std::string transStr = trans.str();
        if (!performSetupReq(videoControlUrl_, transStr)) {
            std::cout << "Failed to send Video SETUP request" << std::endl;
            return false;
        }
    }
    if (!audioControlUrl_.empty()) {
        std::cout << "Sending RTSP SETUP request for audio stream" << std::endl;
        std::stringstream trans;
        trans << "RTP/AVP;unicast;client_port=" << port_+2 << "-" << port_ + 3;
        std::string transStr = trans.str();
        if (!performSetupReq(audioControlUrl_, transStr)) {
            std::cout << "Failed to send Audio SETUP request" << std::endl;
            return false;
        }
    }
    std::cout << "RTSP setup completed successfully" << std::endl;
    return true;
}

bool RtspClient::rtspPlay() {
    const char* range = "npt=now-";
    CURLcode res = CURLE_OK;
    std::string uri = sessionControlUrl_;
    res = curl_easy_setopt(curl_, CURLOPT_RTSP_STREAM_URI, uri.c_str());
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP stream URI option" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_RANGE, range);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Range option" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Request option" << std::endl;
        return false;
    }
    res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        std::cout << "Failed to perform RTSP PLAY request" << std::endl;
        return false;
    }

    // switch off using range again
    res = curl_easy_setopt(curl_, CURLOPT_RANGE, NULL);
    if (res != CURLE_OK) {
        std::cout << "Failed to reset Curl RTSP Range option" << std::endl;
        return false;
    }

    return true;
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

bool RtspClient::parseSdpDescription(std::string& sdp)
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
        if (std::holds_alternative<rtc::Description::Media*>(desc.media(i))) {
            auto m = std::get<rtc::Description::Media*>(desc.media(i));
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
                    videoPayloadType_ = videoCodec_.payloadType();
                }
            }
            else if (m->type() == "audio") {
                audioControlUrl_ = controlUrl;
                if (ptVec.size() > 0) {
                    audioPayloadType_ = ptVec[0];
                }
                else {
                    audioPayloadType_ = audioCodec_.payloadType();
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

std::string RtspClient::parseControlAttribute(std::string& att)
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

bool RtspClient::performSetupReq(std::string& url, std::string& transport)
{
    CURLcode res = CURLE_OK;
    res = curl_easy_setopt(curl_, CURLOPT_RTSP_STREAM_URI, url.c_str());
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP stream URI option" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_RTSP_TRANSPORT, transport.c_str());
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Transport option" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl RTSP Request option" << std::endl;
        return false;
    }
    res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        std::cout << "Failed to perform RTSP SETUP request" << std::endl;
        return false;
    }
    return true;

}

} // namespace
