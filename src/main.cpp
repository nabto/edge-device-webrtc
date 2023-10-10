
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

void parse_options(int argc, char** argv, json& opts);

int main(int argc, char** argv) {
    json opts;
    parse_options(argc, argv, opts);

    auto device = nabto::NabtoDeviceImpl::create(opts);

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

}

void parse_options(int argc, char** argv, json& opts)
{
    try
    {
        cxxopts::Options options(argv[0], "Nabto Webrtc Device example");
        options.add_options()
            ("s,serverurl", "Optional. Server URL for the Nabto basestation", cxxopts::value<std::string>()->default_value("pr-4nmagfvj.devices.dev.nabto.net"))
            ("d,deviceid", "Device ID to connect to", cxxopts::value<std::string>()->default_value("de-sw9unmnn"))
            ("p,productid", "Product ID to use", cxxopts::value<std::string>()->default_value("pr-4nmagfvj"))
            ("t,sct", "Optional. Server connect token from device used for remote connect", cxxopts::value<std::string>()->default_value("demosct"))
            ("loglevel", "Optional. The log level (error|info|trace)", cxxopts::value<std::string>()->default_value("info"))
            ("k,privatekey", "Raw private key to use", cxxopts::value<std::string>()->default_value("b5b45deb271a63071924a219a42b0b67146e50f15e2147c9c5b28f7cf9d1015d"))
            ("r,rtsp", "Use RTSP at the provided url instead of RTP (eg. rtsp://127.0.0.l:8554/video)", cxxopts::value<std::string>())
            ("rtpport", "Port number to use if NOT using RTSP", cxxopts::value<uint16_t>()->default_value("6000"))
            ("j,jwksurl", "URL for jwks server for oAuth", cxxopts::value<std::string>()->default_value("http://localhost:3000/jwks"))
            ("i,jwksissuer", "Issuer of oAuth tokens", cxxopts::value<std::string>()->default_value("http://localhost:3000"))
            ("f,frontendurl", "Frontend URL for pairing link", cxxopts::value<std::string>()->default_value("https://smartcloud.tk.dev.nabto.com/"))
            ("iamreset", "If set, will reset the IAM state before starting")
            ("h,help", "Shows this help text");
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help({ "", "Group" }) << std::endl;
            exit(0);
        }

        opts["productId"] = result["productid"].as<std::string>();
        opts["deviceId"] = result["deviceid"].as<std::string>();
        opts["serverUrl"] = result["serverurl"].as<std::string>();
        opts["rawPrivateKey"] = result["privatekey"].as<std::string>();
        opts["sct"] = result["sct"].as<std::string>();
        opts["logLevel"] = result["loglevel"].as<std::string>();
        if (result.count("rtsp")) {
            opts["rtspUrl"] = result["rtsp"].as<std::string>();
        }
        opts["rtpPort"] = result["rtpport"].as<uint16_t>();
        opts["jwksUrl"] = result["jwksurl"].as<std::string>();
        opts["jwksIssuer"] = result["jwksissuer"].as<std::string>();

        opts["frontendUrl"] = result["frontendurl"].as<std::string>();
        if (result.count("iamreset")) {
            opts["iamReset"] = true;
        } else {
            opts["iamReset"] = false;
        }

    }
    catch (const cxxopts::OptionException& e)
    {
        std::cout << "Error parsing options: " << e.what() << std::endl;
        exit(1);
    }

}
