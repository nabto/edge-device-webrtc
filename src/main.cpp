
#include "event-queue/event_queue_impl.hpp"
#include "signaling-stream/signaling_stream_manager.hpp"
#include "nabto-device/nabto_device.hpp"
#include "media-streams/rtp_stream.hpp"
#include "media-streams/rtsp_stream.hpp"
#include "media-streams/media_stream.hpp"
#include "util.hpp"

#include <rtc/global.hpp>
#include <cxxopts/cxxopts.hpp>
#include <nlohmann/json.hpp>


using nlohmann::json;

bool parse_options(int argc, char** argv, json& opts);

int main(int argc, char** argv) {
    json opts;
    bool shouldExit = parse_options(argc, argv, opts);
    if (shouldExit) {
        return 0;
    }

    auto eventQueue = nabto::EventQueueImpl::create();
    eventQueue->start();

    auto device = nabto::NabtoDeviceImpl::create(opts);

    try {
        bool iamReset = opts["iamReset"].get<bool>();
        if (iamReset) {
            device->resetIam();
            device->stop();
            return 0;
        }
    } catch (std::exception& e) {
        // ignore missing optional option
    }

    if(device == nullptr || !device->start()) {
        std::cout << "Failed to start device" << std::endl;
        return -1;
    }

    auto url = opts["jwksUrl"].get<std::string>();
    auto issuer = opts["jwksIssuer"].get<std::string>();
    device->setJwksConfig(url, issuer);

    std::vector<nabto::MediaStreamPtr> medias;
    nabto::RtspStreamPtr rtsp = nullptr;
    nabto::H264CodecMatcher rtpVideoCodec;
    nabto::OpusCodecMatcher rtpAudioCodec;
    try {
        std::string rtspUrl = opts["rtspUrl"].get<std::string>();
        rtsp = nabto::RtspStream::create("frontdoor", rtspUrl);
        medias.push_back(rtsp);

        // rtsp->start();
        // auto videoRtp = rtsp->getVideoStream();
        // if (videoRtp != nullptr) {
        //     medias.push_back(videoRtp);
        // }
        // auto audioRtp = rtsp->getAudioStream();
        // if (audioRtp != nullptr) {
        //     medias.push_back(audioRtp);
        // }
    } catch (std::exception& ex) {
        // rtspUrl was not set, default to RTP.
        uint16_t port = opts["rtpPort"].get<uint16_t>();
        auto rtp = nabto::RtpStream::create("frontdoor");
        rtp->setVideoConf(port, &rtpVideoCodec);
        rtp->setAudioConf(port+1, &rtpAudioCodec);
        medias.push_back(rtp);
    }

    std::cout << "medias size: " << medias.size() << std::endl;

    auto sigStreamMng = nabto::SignalingStreamManager::create(device, medias);
    if (sigStreamMng == nullptr || !sigStreamMng->start()) {
        std::cout << "Failed to start signaling stream manager" << std::endl;
        return -1;
    }

    nabto::terminationWaiter::waitForTermination();

    device->stop();
    medias.clear();
    if (rtsp != nullptr) {
        rtsp->stop();
    }
    rtsp = nullptr;

    auto fut = rtc::Cleanup();
    fut.get();
    eventQueue->stop();

}

bool parse_options(int argc, char** argv, json& opts)
{
    try
    {
        cxxopts::Options options(argv[0], "Nabto Webrtc Device example");
        options.add_options()
            ("s,serverurl", "Optional. Server URL for the Nabto basestation", cxxopts::value<std::string>())
            ("d,deviceid", "Device ID to connect to", cxxopts::value<std::string>())
            ("p,productid", "Product ID to use", cxxopts::value<std::string>())
            ("log-level", "Optional. The log level (error|info|trace)", cxxopts::value<std::string>()->default_value("info"))
            ("k,privatekey", "Raw private key to use", cxxopts::value<std::string>())
            ("r,rtsp", "Use RTSP at the provided url instead of RTP (eg. rtsp://127.0.0.l:8554/video)", cxxopts::value<std::string>())
            ("rtp-port", "Port number to use if NOT using RTSP", cxxopts::value<uint16_t>()->default_value("6000"))
            ("c,cloud-domain", "Optional. Domain for the cloud deployment. This is used to derive JWKS URL, JWKS issuer, and frontend URL", cxxopts::value<std::string>()->default_value("smartcloud.nabto.com"))
            ("iam-reset", "If set, will reset the IAM state and exit")
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

        if (result.count("serverurl")) {
            opts["serverUrl"] = result["serverurl"].as<std::string>();
        }

        opts["logLevel"] = result["log-level"].as<std::string>();
        if (result.count("rtsp")) {
            opts["rtspUrl"] = result["rtsp"].as<std::string>();
        }
        opts["rtpPort"] = result["rtp-port"].as<uint16_t>();

        std::string domain = result["cloud-domain"].as<std::string>();

        opts["jwksUrl"] = "https://sts." + domain + "/sts/.well-known/jwks.json";
        opts["jwksIssuer"] = "https://sts." + domain + "/sts";

        opts["frontendUrl"] = "https://demo." + domain;
        if (result.count("iam-reset")) {
            opts["iamReset"] = true;
        } else {
            opts["iamReset"] = false;
        }

    }
    catch (const cxxopts::OptionException& e)
    {
        std::cout << "Error parsing options: " << e.what() << std::endl;
        return true;
    }
    return false;
}


