#include "rtsp_client.hpp"

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
    videoStream_ = RtspStream::create(trackId_ + "-video");
    videoStream_->setStreamPort(45222);
    videoStream_->setControlPort(45223);

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
        std::cout << "FAILURE POINT 1" << std::endl;
        return false;
    }
    curl_ = curl_easy_init();
    if (!curl_) {
        std::cout << "FAILURE POINT 2" << std::endl;
        return false;
    }

    res = curl_easy_setopt(curl_, CURLOPT_VERBOSE, 0L);
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 3" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 1L);
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 4" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_HEADERDATA, stdout);
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 5" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_URL, url_.c_str());
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 6" << std::endl;
        return false;
    }
    return true;
}

bool RtspClient::setupRtsp() {
    CURLcode res = CURLE_OK;

    // SENDING OPTIONS REQ
    res = curl_easy_setopt(curl_, CURLOPT_RTSP_STREAM_URI, url_.c_str());
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 7" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_OPTIONS);
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 8" << std::endl;
        return false;
    }
    res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 9" << std::endl;
        return false;
    }

    // SENDING DESCRIBE REQ

    // TODO: This part writes curl data to a file just to read it again immidiately. Replace this with something which writes to a buffer.
    // TODO: In the file, we look for the "a=control:" string to get the control attribute. We then proceed to not use that value and just use the "stream=0" string known to be the control attribute of the video stream. Make a proper sdp parser and read the control attribute of both the video and audio stream.

    FILE* sdp_fp = fopen("test-video.sdp", "wb");
    if (!sdp_fp) {
        std::cout << "FAILURE POINT 10" << std::endl;
        sdp_fp = stdout;
    }
    else {
        std::cout << "Writing SDP to 'test-video.sdp'" << std::endl;
    }

    res = curl_easy_setopt(curl_, CURLOPT_WRITEDATA, sdp_fp);
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 11" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_DESCRIBE);
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 12" << std::endl;
        return false;
    }
    res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 13" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_WRITEDATA, stdout);
    if (sdp_fp != stdout) {
        fclose(sdp_fp);
    }

    char control[256];
    char s[256];
    sdp_fp = fopen("test-video.sdp", "rb");
    control[0] = '\0';
    if (sdp_fp) {
        while (fgets(s, 254, sdp_fp)) {
            sscanf(s, " a = control: %32s", control);
        }
        fclose(sdp_fp);
    }
    std::cout << "Read sdp control: " << control << std::endl;

    // SENDING SETUP REQ for video stream

    // TODO: maybe ephemeral ports

    // std::string ctrlAttr(control);
    // std::string ctrlAttr("video");
    std::string ctrlAttr("stream=0");
    std::string setupUri = url_ + "/" + ctrlAttr;
    std::string transport = "RTP/AVP;unicast;client_port=45222-45223";


    res = curl_easy_setopt(curl_, CURLOPT_RTSP_STREAM_URI, setupUri.c_str());
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 14" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_RTSP_TRANSPORT, transport.c_str());
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 15" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_SETUP);
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 16" << std::endl;
        return false;
    }
    res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 17" << std::endl;
        return false;
    }

    // TODO: SENDING SETUP REQ for audio stream
    return true;
}

bool RtspClient::rtspPlay() {
    const char* range = "npt=now-";
    CURLcode res = CURLE_OK;
    std::string uri = url_ + "/";
    res = curl_easy_setopt(curl_, CURLOPT_RTSP_STREAM_URI, uri.c_str());
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 18" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_RANGE, range);
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 19" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_RTSP_REQUEST, (long)CURL_RTSPREQ_PLAY);
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 20" << std::endl;
        return false;
    }
    res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 21" << std::endl;
        return false;
    }

    // switch off using range again
    res = curl_easy_setopt(curl_, CURLOPT_RANGE, NULL);
    if (res != CURLE_OK) {
        std::cout << "FAILURE POINT 22" << std::endl;
        return false;
    }

    return true;
}


} // namespace
