#include <rtsp-client/rtsp_client.hpp>
#include <track-negotiators/h264.hpp>
#include <track-negotiators/opus.hpp>

namespace terminationWaiter {

std::promise<void> promise_;

void signal_handler(int s)
{
    (void)s;
    promise_.set_value();

}

void waitForTermination() {
    signal(SIGINT, &signal_handler);

    std::future<void> f = promise_.get_future();
    f.get();

}
} // namespace terminationWaiter

int main(int argc, char** argv) {

    auto rtsp = nabto::RtspClient::create("footrack", "rtsp://user:password@127.0.0.1:8554/video");
    auto rtpVideoNegotiator = nabto::H264Negotiator::create();
    auto rtpAudioNegotiator = nabto::OpusNegotiator::create();
    rtsp->setTrackNegotiators(rtpVideoNegotiator, rtpAudioNegotiator);

    rtsp->start([](CURLcode res, uint16_t statuscode) {
        std::cout << "Returned curlcode: " << res << " statuscode: " << statuscode << std::endl;
    });

    terminationWaiter::waitForTermination();

}
