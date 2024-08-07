#include "coap_listener.hpp"

#include <nabto/nabto_device_webrtc.hpp>

#include <event-queue/event_queue_impl.hpp>
#include <rtp-client/rtp_client.hpp>
#include <track-negotiators/h264.hpp>
#include <util/util.hpp>

#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>

#include <cxxopts.hpp>
#include <rtc/global.hpp>
#include <nlohmann/json.hpp>
#include <signal.h>

using nlohmann::json;

const char* coapFeedPath[] = { "webrtc", "get", NULL };
std::string trackId = "frontdoor-video";

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

bool parse_options(int argc, char** argv, json& opts);

bool initDevice(nabto::NabtoDevicePtr device, json& opts);

enum plog::Severity plogSeverity(std::string& logLevel)
{
    if (logLevel == "trace") {
        return plog::Severity::debug;
    }
    else if (logLevel == "warn") {
        return plog::Severity::warning;
    }
    else if (logLevel == "info") {
        return plog::Severity::info;
    }
    else if (logLevel == "error") {
        return plog::Severity::error;
    }
    return plog::Severity::none;

}


int main(int argc, char** argv) {

    json opts;
    bool shouldExit = parse_options(argc, argv, opts);
    if (shouldExit) {
        return 0;
    }

    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;

    auto logLevel = opts["logLevel"].get<std::string>();
    char* envLvl = std::getenv("NABTO_WEBRTC_LOG_LEVEL");
    if (envLvl != NULL) {
        logLevel = std::string(envLvl);
    }
    nabto::initLogger(plogSeverity(logLevel), &consoleAppender);


    auto device = nabto::makeNabtoDevice();

    if (!initDevice(device, opts)){
        return 1;
    }

    auto rtpVideoNegotiator = nabto::H264Negotiator::create();
    uint16_t port = opts["rtpPort"].get<uint16_t>();
    nabto::RtpClientConf conf = {trackId, std::string(), port, rtpVideoNegotiator, nullptr};
    auto rtpVideo = nabto::RtpClient::create(conf);

    auto eventQueue = nabto::EventQueueImpl::create();

    auto coapListener = CoapListener::create(device, NABTO_DEVICE_COAP_GET, coapFeedPath, eventQueue);

    auto webrtc = nabto::NabtoDeviceWebrtc::create(eventQueue, device);
    webrtc->setCheckAccessCallback([](NabtoDeviceConnectionRef ref, std::string action) -> bool {
        return true;
        });

    coapListener->setCoapCallback([webrtc, device, rtpVideo, rtpVideoNegotiator](NabtoDeviceCoapRequest* coap) {
        std::cout << "Got new coap request" << std::endl;

        NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

        std::vector<nabto::MediaTrackPtr> list;

        auto media = rtpVideo->createMedia(trackId);
        media->setCloseCallback([rtpVideo, ref]() {
            rtpVideo->removeConnection(ref);
        });
        rtpVideo->addConnection(ref, media);
        list.push_back(media);

        if (!webrtc->connectionAddMediaTracks(ref, list)) {
            std::cout << "Failed to add medias to connection" << std::endl;
            nabto_device_coap_error_response(coap, 500, "Internal Server Error");
            return;
        }

        nabto_device_coap_response_set_code(coap, 201);
        nabto_device_coap_response_ready(coap);
        nabto_device_coap_request_free(coap);
    });

    webrtc->setDatachannelEventCallback([](NabtoDeviceConnectionRef ref, nabto::DatachannelPtr channel) {
        std::cout << "Got new datachannel with label: " << channel->getLabel() << std::endl;
        std::string data = "helloWorld";
        channel->setMessageCallback([channel](nabto::Datachannel::MessageType type, uint8_t* buffer, size_t length) {
            std::cout << "Received datachannel message! type: " << type << " message: " << std::string((char*)buffer, length) << std::endl;
        });
        channel->sendMessage((uint8_t*)data.data(), data.size());
    });


    sigContext = std::make_shared<SigIntContext>(eventQueue, device);
    signal(SIGINT, &signal_handler);

    eventQueue->run();

    auto fut = rtc::Cleanup();
    fut.get();

    return 0;
}


bool initDevice(nabto::NabtoDevicePtr device, json& opts)
{
    char* fp;
    NabtoDeviceError ec;
    if (nabto_device_set_log_std_out_callback(NULL) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_log_level(NULL, opts["logLevel"].get<std::string>().c_str()) != NABTO_DEVICE_EC_OK)
    {
        std::cout << "failed to set loglevel or logger" << std::endl;
        return false;
    }

    uint8_t key[32];
    std::string rawPrivateKey = opts["rawPrivateKey"].get<std::string>();
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

    std::cout << "################################################################" << std::endl
        << "# Device configuration string: " << std::endl
        << "# productid:" << opts["productId"].get<std::string>() << ",deviceId:" << opts["deviceId"].get<std::string>() << ",sct:" << opts["sct"].get<std::string>() << std::endl
        << "################################################################" << std::endl
        << "The device fingerprint: [" << fp << "]" << std::endl;
    nabto_device_string_free(fp);

    if (nabto_device_set_product_id(device.get(), opts["productId"].get<std::string>().c_str()) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_device_id(device.get(), opts["deviceId"].get<std::string>().c_str()) != NABTO_DEVICE_EC_OK ||
        nabto_device_enable_mdns(device.get()) != NABTO_DEVICE_EC_OK ||
        nabto_device_add_server_connect_token(device.get(), opts["sct"].get<std::string>().c_str()) != NABTO_DEVICE_EC_OK)
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

bool parse_options(int argc, char** argv, json& opts)
{
    try
    {
        cxxopts::Options options(argv[0], "Nabto Webrtc Device example");
        options.add_options()
            ("d,deviceid", "Device ID to connect to", cxxopts::value<std::string>())
            ("p,productid", "Product ID to use", cxxopts::value<std::string>())
            ("log-level", "Optional. The log level (error|info|trace)", cxxopts::value<std::string>()->default_value("info"))
            ("k,privatekey", "Raw private key to use", cxxopts::value<std::string>())
            ("rtp-port", "Port number to use if NOT using RTSP", cxxopts::value<uint16_t>()->default_value("6000"))
            ("sct", "SCT to use", cxxopts::value<std::string>()->default_value("demosct"))
            ("create-key", "If set, will create and print a raw private key and its fingerprint. Then exit")
            ("h,help", "Shows this help text");
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help({ "", "Group" }) << std::endl;
            return true;
        }

        if (result.count("create-key")) {
            nabto::PrivateKeyCreator();
            return true;
        }

        if (!result.count("productid") ||
            !result.count("deviceid") ||
            !result.count("privatekey"))
        {
            std::cout << "Missing required argument" << std::endl;
            std::cout << options.help({ "", "Group" }) << std::endl;
            return true;
        }
        opts["productId"] = result["productid"].as<std::string>();
        opts["deviceId"] = result["deviceid"].as<std::string>();
        opts["rawPrivateKey"] = result["privatekey"].as<std::string>();
        opts["sct"] = result["sct"].as<std::string>();

        opts["logLevel"] = result["log-level"].as<std::string>();
        if (result.count("rtsp")) {
            opts["rtspUrl"] = result["rtsp"].as<std::string>();
        }
        opts["rtpPort"] = result["rtp-port"].as<uint16_t>();

    } catch (const cxxopts::exceptions::exception& e)
    {
        std::cout << "Error parsing options: " << e.what() << std::endl;
        return true;
    }
    return false;

}
