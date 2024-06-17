#include "nabto_device.hpp"

#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>

#include <event-queue/event_queue_impl.hpp>
#include <util/util.hpp>
#include <media-streams/media_stream.hpp>
#include <track-negotiators/h264.hpp>
#include <track-negotiators/opus.hpp>
#include <rtp-packetizer/h264_packetizer.hpp>
#include <rtp-repacketizer/h264_repacketizer.hpp>
#include <rtp-client/rtp_client.hpp>
#include <rtsp-client/rtsp_stream.hpp>
#include <fifo-file-client/fifo_file_client.hpp>
#include <nabto/nabto_device_webrtc.hpp>

#include <rtc/global.hpp>

#include <cxxopts/cxxopts.hpp>
#include <nlohmann/json.hpp>
#include <signal.h>
#include <cstdlib>


using nlohmann::json;

// TODO: remove legacy endpoint
const char* coapVideoPath[] = { "webrtc", "video", "frontdoor", NULL };

const char* coapTracksPath[] = { "webrtc", "tracks", NULL };
const char* coapTracksDefaultPath[] = { "webrtc", "tracks", "default", NULL };

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
enum plog::Severity plogSeverity(std::string& level);

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

    std::vector<nabto::MediaStreamPtr> medias;
    nabto::RtspStreamPtr rtsp = nullptr;
    nabto::FifoFileClientPtr fifo = nullptr;
    bool repacketH264 = opts["repacketH264"].get<bool>();
    auto rtpVideoNegotiator = nabto::H264Negotiator::create();
    auto rtpAudioNegotiator = nabto::OpusNegotiator::create();

    try {
        std::string rtspUrl = opts["rtspUrl"].get<std::string>();
        nabto::RtspStreamConf conf = { "frontdoor", rtspUrl, rtpVideoNegotiator, rtpAudioNegotiator, nullptr, nullptr};
        if (repacketH264) {
            conf.videoRepack = nabto::H264RepacketizerFactory::create();
        }
        rtsp = nabto::RtspStream::create(conf);
        medias.push_back(rtsp);
    } catch (std::exception& ex) {
        // rtspUrl was not set, try fifo
        try {
            auto fifoPacketizer = nabto::H264PacketizerFactory::create("frontdoor-video");
            std::string fifoPath = opts["fifoPath"].get<std::string>();

            nabto::FifoFileClientConf conf = { "frontdoor-video", fifoPath, rtpVideoNegotiator, fifoPacketizer };
            fifo = nabto::FifoFileClient::create(conf);

            medias.push_back(fifo);

        } catch (std::exception& ex) {
            // fifoPath was not set, default to RTP.
            uint16_t port = opts["rtpPort"].get<uint16_t>();
            nabto::RtpClientConf videoConf = { "frontdoor-video", "127.0.0.1", port, rtpVideoNegotiator, nullptr };
            if (repacketH264) {
                videoConf.repacketizer = nabto::H264RepacketizerFactory::create();
            }

            auto rtpVideo = nabto::RtpClient::create(videoConf);

            medias.push_back(rtpVideo);

            nabto::RtpClientConf audioConf = { "frontdoor-audio", "127.0.0.1", (uint16_t)(port + 2), rtpAudioNegotiator, nullptr };
            auto rtpAudio = nabto::RtpClient::create(audioConf);

            medias.push_back(rtpAudio);
        }
    }

    std::cout << "medias size: " << medias.size() << std::endl;
    std::cout << "Nabto Device WebRTC version: " << nabto::NabtoDeviceWebrtc::version() << std::endl;

    auto webrtc = nabto::NabtoDeviceWebrtc::create(eventQueue, device->getDevice());
    webrtc->setCheckAccessCallback([device](NabtoDeviceConnectionRef ref, std::string action) -> bool {
        return nm_iam_check_access(device->getIam(), ref, action.c_str(), NULL);
    });

    webrtc->setTrackEventCallback([device, medias, rtsp](NabtoDeviceConnectionRef connRef, nabto::MediaTrackPtr track) {
        if (!nm_iam_check_access(device->getIam(), connRef, "Webrtc:VideoStream", NULL)) {
            track->setErrorState(nabto::MediaTrack::ErrorState::ACCESS_DENIED);
            return;
        }
        auto feed = track->getTrackId();
        if (feed.empty()) {
            std::cout << "Received track event for track without track ID!" << std::endl;
            track->setErrorState(nabto::MediaTrack::ErrorState::UNKNOWN_TRACK_ID);
            return;
        }
        bool found = false;
        for (auto m : medias) {
            if (m->isTrack(feed)) {
                found = true;
                if (!m->matchMedia(track)) {
                    std::cout << "Codec mismatch for track: " << feed << std::endl;
                    track->setErrorState(nabto::MediaTrack::ErrorState::INVALID_CODECS);
                    return;
                }
                NabtoDeviceConnectionRef ref = connRef;
                m->addConnection(ref, track);
                track->setCloseCallback([m, ref]() {
                    m->removeConnection(ref);
                    });
            }
        }
        if (!found) {
            track->setErrorState(nabto::MediaTrack::ErrorState::UNKNOWN_TRACK_ID);
        }
    });

    auto coapTracksListener = example::NabtoDeviceCoapListener::create(device, NABTO_DEVICE_COAP_POST, coapTracksPath, eventQueue);

    coapTracksListener->setCoapCallback([webrtc, device, medias](NabtoDeviceCoapRequest* coap) {
        NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

        if (nm_iam_check_access(device->getIam(), ref, "Webrtc:VideoStream", NULL)) {
            NabtoDeviceError ec;
            uint16_t ct;
            char* payload;
            size_t payloadLen;
            if (
                nabto_device_coap_request_get_content_format(coap, &ct) != NABTO_DEVICE_EC_OK ||
                ct != NABTO_DEVICE_COAP_CONTENT_FORMAT_APPLICATION_JSON ||
                nabto_device_coap_request_get_payload(coap, (void**)&payload, &payloadLen) != NABTO_DEVICE_EC_OK
            ) {
                nabto_device_coap_error_response(coap, 400, "Invalid Request. Missing/invalid payload/content format");
            } else {
                std::vector<std::string> tracks;
                try {
                    auto jsonStr = std::string(payload, payloadLen);
                    nlohmann::json jsonPl = nlohmann::json::parse(jsonStr);
                    tracks = jsonPl["tracks"].get<std::vector<std::string>>();
                } catch (nlohmann::json::exception& ex) {
                    nabto_device_coap_error_response(coap, 400, "Invalid Request. JSON parse error");
                    nabto_device_coap_request_free(coap);
                    return;
                }

                bool found = false;
                std::vector<nabto::MediaTrackPtr> list;

                for (auto m : medias) {
                    for (auto feed : tracks) {
                        if (m->isTrack(feed)) {
                            found = true;
                            auto media = m->createMedia(feed);
                            media->setCloseCallback([m, ref]() {
                                m->removeConnection(ref);
                                });
                            m->addConnection(ref, media);
                            list.push_back(media);
                            break;
                        }
                    }
                }
                if (!webrtc->connectionAddMediaTracks(ref, list)) {
                    std::cout << "Failed to add medias to connection" << std::endl;
                    nabto_device_coap_error_response(coap, 500, "Internal Server Error");
                    nabto_device_coap_request_free(coap);
                    return;
                }

                if (found) {
                    nabto_device_coap_response_set_code(coap, 201);
                    nabto_device_coap_response_ready(coap);
                }
                else {
                    std::cout << "Failed to find track for video feed" << std::endl;
                    nabto_device_coap_error_response(coap, 404, "No such feed");
                }
            }
        }
        else {
            std::cout << "Got CoAP video stream request but IAM rejected it" << std::endl;
            nabto_device_coap_error_response(coap, 401, NULL);
        }

        nabto_device_coap_request_free(coap);

        });


    auto coapTracksDefListener = example::NabtoDeviceCoapListener::create(device, NABTO_DEVICE_COAP_GET, coapTracksDefaultPath, eventQueue);

    coapTracksDefListener->setCoapCallback([device](NabtoDeviceCoapRequest* coap) {
        NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

        if (nm_iam_check_access(device->getIam(), ref, "Webrtc:VideoStream", NULL)) {
            nlohmann::json resp = { {"videoTrackId", "frontdoor-video"},{"audioTrackId", "frontdoor-audio"} };
            auto respPayload = resp.dump(); //nlohmann::json::to_cbor(resp);
            nabto_device_coap_response_set_code(coap, 205);
            nabto_device_coap_response_set_content_format(coap, NABTO_DEVICE_COAP_CONTENT_FORMAT_APPLICATION_JSON);
            nabto_device_coap_response_set_payload(coap, respPayload.data(), respPayload.size());
            nabto_device_coap_response_ready(coap);
            nabto_device_coap_request_free(coap);
        } else {
            nabto_device_coap_error_response(coap, 401, NULL);
            nabto_device_coap_request_free(coap);
        }
    });


    // TODO: remove this legacy coap listener
    auto coapVideoListener = example::NabtoDeviceCoapListener::create(device, NABTO_DEVICE_COAP_GET, coapVideoPath, eventQueue);

    coapVideoListener->setCoapCallback([webrtc, device, medias](NabtoDeviceCoapRequest* coap) {

        NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

        if (nm_iam_check_access(device->getIam(), ref, "Webrtc:VideoStream", NULL)) {

            bool found = false;
            std::vector<nabto::MediaTrackPtr> list;
            std::string t1 = "frontdoor-video";
            std::string t2 = "frontdoor-audio";

            for (auto m : medias) {
                nabto::MediaTrackPtr media = nullptr;
                if (m->isTrack(t1)) {
                    media = m->createMedia(t1);
                } else if (m->isTrack(t2) ) {
                    media = m->createMedia(t2);
                } else {
                    continue;
                }
                found = true;
                media->setCloseCallback([m, ref]() {
                    m->removeConnection(ref);
                });
                m->addConnection(ref, media);
                list.push_back(media);
            }
            if (!webrtc->connectionAddMediaTracks(ref, list)) {
                std::cout << "Failed to add medias to connection" << std::endl;
                nabto_device_coap_error_response(coap, 500, "Internal Server Error");
            } else if (found) {
                nabto_device_coap_response_set_code(coap, 201);
                nabto_device_coap_response_ready(coap);
            } else {
                std::cout << "Failed to find track for video feed" << std::endl;
                nabto_device_coap_error_response(coap, 404, "No such feed");
            }
        } else {
            std::cout << "Got CoAP video stream request but IAM rejected it" << std::endl;
            nabto_device_coap_error_response(coap, 401, NULL);
        }

        nabto_device_coap_request_free(coap);

        });


    sigContext = std::make_shared<SigIntContext>(eventQueue, device);
    signal(SIGINT, &signal_handler);

    eventQueue->run();

    medias.clear();
    if (rtsp != nullptr) {
        rtsp->stop();
    }
    rtsp = nullptr;

    auto fut = rtc::Cleanup();
    fut.wait();
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
            ("log-level", "Optional. The log level (error|info|trace)", cxxopts::value<std::string>()->default_value("error"))
            ("k,privatekey", "Raw private key to use", cxxopts::value<std::string>())
            ("r,rtsp", "Use RTSP at the provided url instead of RTP (eg. rtsp://127.0.0.l:8554/video)", cxxopts::value<std::string>())
            ("rtp-port", "Port number to use if NOT using RTSP", cxxopts::value<uint16_t>()->default_value("6000"))
            ("f,fifo", "Use FIFO file descriptor at the provided path instead of RTP", cxxopts::value<std::string>())
            ("c,cloud-domain", "Optional. Domain for the cloud deployment. This is used to derive JWKS URL, JWKS issuer, and frontend URL", cxxopts::value<std::string>()->default_value("smartcloud.nabto.com"))
            ("H,home-dir", "Set which dir to store IAM data", cxxopts::value<std::string>())
            ("iam-reset", "If set, will reset the IAM state and exit")
            ("create-key", "If set, will create and print a raw private key and its fingerprint. Then exit")
            /*
            * The cacert option adds an additioanl cacert file to the set of
            * files used by curl. Setting the CURLOPT_CAINFO does not override
            * CURLOPT_CAPATH. The combined set of certificates in use will be
            * both the certificates in CAPATH and the certificates in CAINFO.
            */
            ("cacert", "Optional. Path to a CA certificate file; overrides CURL_CA_BUNDLE env var if set.", cxxopts::value<std::string>())
            ("disable-h264-repacketizer", "If set, H264 will be forwarded as-is instead of repacketizing to proper MTU")

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

        if (result.count("home-dir")) {
            opts["homeDir"] = result["home-dir"].as<std::string>();
        }

        std::string ll = result["log-level"].as<std::string>();
        if (ll == "error" || ll == "info" || ll == "trace") {
            opts["logLevel"] = ll;
        } else {
            std::cout << "Invalid log level specified. expected: error|info|trace got: " << ll << std::endl;
            return true;
        }

        if (result.count("rtsp")) {
            opts["rtspUrl"] = result["rtsp"].as<std::string>();
        }
        opts["rtpPort"] = result["rtp-port"].as<uint16_t>();
        if (result.count("fifo")) {
            opts["fifoPath"] = result["fifo"].as<std::string>();
        }

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

        if (result.count("cacert")) {
            opts["caBundle"] = result["cacert"].as<std::string>();
        } else {
            const char* curlCaBundle = std::getenv("CURL_CA_BUNDLE");
            if (curlCaBundle != nullptr) {
                opts["caBundle"] = std::string(curlCaBundle);
            }
        }
        if (opts.contains("caBundle")) {
            std::ifstream f(opts["caBundle"].get<std::string>());
            if (!f.good()) {
                std::cout << "CA certificate bundle file does not exist: " << opts["caBundle"].get<std::string>() << std::endl;
                return true;
            }
        }
        if (result.count("disable-h264-repacketizer")) {
            opts["repacketH264"] = false;
        }
        else {
            opts["repacketH264"] = true;
        }


    } catch (const cxxopts::OptionException& e)
    {
        std::cout << "Error parsing options: " << e.what() << std::endl;
        return true;
    }
    return false;
}

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
