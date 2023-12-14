#include "nabto_device.hpp"

#include <event-queue/event_queue_impl.hpp>
#include <media-streams/media_stream.hpp>
#include <media-streams/rtsp_stream.hpp>
#include <media-streams/rtp_stream.hpp>
#include <nabto/nabto_device_webrtc.hpp>

#include <rtc/global.hpp>

#include <cxxopts/cxxopts.hpp>
#include <nlohmann/json.hpp>
#include <signal.h>

using nlohmann::json;

const char* coapVideoPath[] = { "webrtc", "video", "{feed}", NULL };


class SigIntContext {
public:
    SigIntContext(nabto::EventQueueImplPtr q, example::NabtoDeviceAppPtr d)
        : queue(q), work(q), device(d) {}
    ~SigIntContext()
    {
        auto d = device;
        auto q = queue;
        queue->post([d, q]() {
            d->stop();
            });
    }
private:
    nabto::EventQueueImplPtr queue = nullptr;
    nabto::EventQueueWork work;
    example::NabtoDeviceAppPtr device = nullptr;

};

std::shared_ptr<SigIntContext> sigContext = nullptr;

void signal_handler(int s)
{
    (void)s;
    sigContext = nullptr;
}

bool parse_options(int argc, char** argv, json& opts);

int main(int argc, char** argv) {
    json opts;
    bool shouldExit = parse_options(argc, argv, opts);
    if (shouldExit) {
        return 0;
    }

    auto eventQueue = nabto::EventQueueImpl::create();
    auto device = example::NabtoDeviceApp::create(opts, eventQueue);

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

    if (device == nullptr || !device->start()) {
        std::cout << "Failed to start device" << std::endl;
        if (device) {
            device->stop();
        }
        return -1;
    }

    auto url = opts["jwksUrl"].get<std::string>();
    auto issuer = opts["jwksIssuer"].get<std::string>();
    device->setJwksConfig(url, issuer);

    // TODO: remake for new API
    std::vector<nabto::RtpClientPtr> medias;
    nabto::RtspStreamPtr rtsp = nullptr;
    nabto::H264CodecMatcher rtpVideoCodec;
    nabto::OpusCodecMatcher rtpAudioCodec;
    // try {
        // std::string rtspUrl = opts["rtspUrl"].get<std::string>();
        // rtsp = nabto::RtspStream::create("frontdoor", rtspUrl);
        // medias.push_back(rtsp);

    // } catch (std::exception& ex) {
        // rtspUrl was not set, default to RTP.
        uint16_t port = opts["rtpPort"].get<uint16_t>();

        auto rtpVideo = nabto::RtpClient::create("frontdoor-video");
        rtpVideo->setPort(port);
        rtpVideo->setRtpCodecMatcher(&rtpVideoCodec);
        // Remote host is only used for 2-way medias, video is only 1-way
        // rtpVideo->setRemoteHost("127.0.0.1");

        medias.push_back(rtpVideo);
    // }

    std::cout << "medias size: " << medias.size() << std::endl;

    auto coapVideoListener = example::NabtoDeviceCoapListener::create(device, NABTO_DEVICE_COAP_GET, coapVideoPath, eventQueue);

    auto webrtc = nabto::NabtoDeviceWebrtc::create(eventQueue, device->getDevice());
    webrtc->setCheckAccessCallback([device](NabtoDeviceConnectionRef ref, std::string action) -> bool {
        return nm_iam_check_access(device->getIam(), ref, action.c_str(), NULL);
    });

    // TODO: tracks should be split into individual audio/video tracks, and this EP should take a list of trackIds in the payload and add all tracks instead of only adding video (the old bundling style does not work with new api)
    coapVideoListener->setCoapCallback([webrtc, device, medias](NabtoDeviceCoapRequest* coap) {
        std::cout << "Got new coap request" << std::endl;

        NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

        if (nm_iam_check_access(device->getIam(), ref, "Webrtc:VideoStream", NULL)) {
            std::cout << "Handling video coap request" << std::endl;
            const char* feedC = nabto_device_coap_request_get_parameter(coap, "feed");
            std::string feed(feedC);
            NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

            bool found = false;
            std::vector<nabto::MediaTrackPtr> list;

            for (auto m : medias) {
                auto id = m->getTrackId();
                if (id == feed || id == (feed + "-video") || id == (feed + "-audio")) {
                    found = true;
                    auto sdp = m->sdp();
                    auto media = nabto::MediaTrack::create(id, sdp);
                    auto c = m->getRtpCodecMatcher();
                    const rtc::SSRC ssrc = c->ssrc();
                    nabto::RtpTrack track = {
                        media,
                        ssrc,
                        c->payloadType(),
                        c->payloadType()
                    };
                    m->addConnection(ref, track);

                    list.push_back(media);
                }
            }
            if (!webrtc->connectionAddMedias(ref, list)) {
                std::cout << "Failed to add medias to connection" << std::endl;
                nabto_device_coap_error_response(coap, 500, "Internal Server Error");
                return;
            }

            if (found) {
                std::cout << "found track ID in feeds returning 201" << std::endl;
                nabto_device_coap_response_set_code(coap, 201);
                nabto_device_coap_response_ready(coap);
            }
            else {
                std::cout << "Failed to find track for video feed" << std::endl;
                nabto_device_coap_error_response(coap, 404, "No such feed");
            }
        }
        else {
            bool pwd = nabto_device_connection_is_password_authenticated(device->getDevice().get(), ref);
            char* username;

            if (pwd) {
                nabto_device_connection_get_password_authentication_username(device->getDevice().get(), ref, &username);
            }

            std::cout << "Got CoAP video stream request but IAM rejected it" << std::endl << "  was pwd: " << pwd << std::endl << "  username: " << (pwd ? username : "") << std::endl;
            if (pwd) {
                nabto_device_string_free(username);
            }
            nabto_device_coap_error_response(coap, 401, NULL);
        }

        std::cout << "Freeing coap request" << std::endl;
        nabto_device_coap_request_free(coap);

        });


    sigContext = std::make_shared<SigIntContext>(eventQueue, device);
    signal(SIGINT, &signal_handler);

    eventQueue->run();

    // medias.clear();
    // if (rtsp != nullptr) {
    //     rtsp->stop();
    // }
    // rtsp = nullptr;

    auto fut = rtc::Cleanup();
    fut.get();

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
        }
        else {
            opts["iamReset"] = false;
        }

    } catch (const cxxopts::OptionException& e)
    {
        std::cout << "Error parsing options: " << e.what() << std::endl;
        return true;
    }
    return false;
}


