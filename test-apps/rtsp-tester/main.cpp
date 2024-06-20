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
    if (argc != 2) {
        std::cout << "Usage: " << std::endl << "  " << argv[0] << " <RTSP_URL>" << std::endl;
        return -1;
    }

    std::string url(argv[1]);

    auto rtpVideoNegotiator = nabto::H264Negotiator::create();
    auto rtpAudioNegotiator = nabto::OpusNegotiator::create();
    nabto::RtspClientConf conf = {
        "footrack",
        url,
        rtpVideoNegotiator,
        rtpAudioNegotiator,
        nullptr,
        nullptr
    };
    auto rtsp = nabto::RtspClient::create(conf);

    rtsp->start([](std::optional<std::string> error) {
        if (error.has_value()) {
            std::cout << "Returned error string: " << error.value() << std::endl;
        } else {
            std::cout << "SUCCESS!" << std::endl;
        }
    });

    terminationWaiter::waitForTermination();

}
