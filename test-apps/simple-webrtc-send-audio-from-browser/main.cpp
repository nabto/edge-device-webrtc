#include "coap_listener.hpp"

#include <nabto/nabto_device_webrtc.hpp>

#include <nabto/nabto_device_experimental.h>

#include <event-queue/event_queue_impl.hpp>
#include <rtp-client/rtp_client.hpp>
#include <track-negotiators/h264.hpp>
#include <track-negotiators/opus.hpp>

#include <rtc/global.hpp>
#include <nlohmann/json.hpp>
#include <signal.h>
#include <cxxopts.hpp>

using nlohmann::json;


class ProgramOptions {
public:
    std::string productId;
    std::string deviceId;
    std::string rawPrivateKey;
    std::string sct = "demosct";
    std::string logLevel = "info";
};

bool parse_options(int argc, char **argv, ProgramOptions &opts);

class SigIntContext {
public:
    SigIntContext(nabto::EventQueueImplPtr q, nabto::NabtoDevicePtr d)
        : queue(q), work(q), device(d) {}
    ~SigIntContext()
    {
        auto d = device;
        auto q = queue;
        queue->post([d, q]() {
            nabto_device_stop(d.get());
            });
    }
private:
    nabto::EventQueueImplPtr queue = nullptr;
    nabto::EventQueueWork work;
    nabto::NabtoDevicePtr device = nullptr;

};

std::shared_ptr<SigIntContext> sigContext = nullptr;

void signal_handler(int s)
{
    (void)s;
    sigContext = nullptr;
}


bool initDevice(nabto::NabtoDevicePtr device, const ProgramOptions& opts);

int main(int argc, char** argv) {

    ProgramOptions opts;

    if (!parse_options(argc, argv, opts)) {
        exit(1);
    }

    auto device = nabto::makeNabtoDevice();

    if (!initDevice(device, opts)){
        return 1;
    }

    auto rtpVideoNegotiator_ = nabto::OpusNegotiator::create();
    nabto::RtpClientConf conf = { "frontdoor-audio", std::string(), 6002, rtpVideoNegotiator_, nullptr };

    auto rtpVideo = nabto::RtpClient::create(conf);

    auto eventQueue = nabto::EventQueueImpl::create();

    auto webrtc = nabto::NabtoDeviceWebrtc::create(eventQueue, device);
    webrtc->setTrackEventCallback(
        [rtpVideo](NabtoDeviceConnectionRef ref, nabto::MediaTrackPtr track)
        {
            const std::string &trackId = track->getTrackId();
            if (trackId == "frontdoor-audio") {
                auto ok = rtpVideo->matchMedia(track);
                if (!ok) {
                    // TODO
                }

                track->setCloseCallback([rtpVideo, ref]() { rtpVideo->removeConnection(ref); });
                rtpVideo->addConnection(ref, track);
            } else {
                track->setErrorState(nabto::MediaTrack::ErrorState::UNKNOWN_TRACK_ID);
            }
        });

    webrtc->setCheckAccessCallback([](NabtoDeviceConnectionRef ref, std::string action) -> bool
                                   { return true; });

    sigContext = std::make_shared<SigIntContext>(eventQueue, device);
    signal(SIGINT, &signal_handler);

    eventQueue->run();

    auto fut = rtc::Cleanup();
    fut.get();

    return 0;
}


bool initDevice(nabto::NabtoDevicePtr device, const ProgramOptions& opts)
{
    char* fp;
    NabtoDeviceError ec;
    if (nabto_device_set_log_std_out_callback(NULL) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_log_level(NULL, opts.logLevel.c_str()) != NABTO_DEVICE_EC_OK)
    {
        std::cout << "failed to set loglevel or logger" << std::endl;
        return false;
    }

    uint8_t key[32];
    std::string rawPrivateKey = opts.rawPrivateKey;
    for (size_t i = 0; i < 32; i++) {
        std::string s(&rawPrivateKey[i * 2], 2);
        key[i] = std::stoi(s, 0, 16);
    }

    if ((ec = nabto_device_set_private_key_secp256r1(device.get(), key, 32)) != NABTO_DEVICE_EC_OK) {
        std::cout << "Failed to set private key, ec=" << nabto_device_error_get_message(ec) << std::endl;
        return false;
    }

    if (nabto_device_get_device_fingerprint(device.get(), &fp) != NABTO_DEVICE_EC_OK) {
        return false;
    }

    std::cout << "Device: " << opts.productId << "." << opts.deviceId << " with fingerprint: [" << fp << "]" << std::endl;;
    nabto_device_string_free(fp);

    if (nabto_device_set_product_id(device.get(), opts.productId.c_str()) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_device_id(device.get(), opts.deviceId.c_str()) != NABTO_DEVICE_EC_OK ||
        nabto_device_enable_mdns(device.get()) != NABTO_DEVICE_EC_OK ||
        nabto_device_add_server_connect_token(device.get(), opts.sct.c_str()) != NABTO_DEVICE_EC_OK)
    {
        return false;
    }


    NabtoDeviceFuture* fut = nabto_device_future_new(device.get());
    nabto_device_start(device.get(), fut);

    ec = nabto_device_future_wait(fut);
    nabto_device_future_free(fut);
    if (ec != NABTO_DEVICE_EC_OK) {
        std::cout << "Failed to start device, ec=" << nabto_device_error_get_message(ec) << std::endl;
        return false;
    }
    return true;
}


bool parse_options(int argc, char** argv, ProgramOptions& opts)
{
    try
    {
        cxxopts::Options options(argv[0], "Nabto Webrtc Device example");
        options.add_options()
            ("d,deviceid", "Device ID to connect to", cxxopts::value<std::string>(opts.deviceId))
            ("p,productid", "Product ID to use", cxxopts::value<std::string>(opts.productId))
            ("log-level", "Optional. The log level (error|info|trace)", cxxopts::value<std::string>(opts.logLevel)->default_value("info"))
            ("k,privatekey", "Raw private key to use", cxxopts::value<std::string>(opts.rawPrivateKey))
            ("h,help", "Print help")
            ;
        auto result = options.parse(argc, argv);
        if (result.count("help")) {
            std::cout << options.help({ "", "Group" }) << std::endl;
            exit(0);
        }
        return true;
    }
    catch (const cxxopts::exceptions::exception &e)
    {
        std::cout << "Error parsing options: " << e.what() << std::endl;
        return false;
    }
}
