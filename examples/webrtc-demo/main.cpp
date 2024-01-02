#include "nabto_device.hpp"

#include <event-queue/event_queue_impl.hpp>
#include <util/util.hpp>
#include <media-streams/media_stream.hpp>
#include <rtp-client/rtp_client.hpp>
#include <rtsp-client/rtsp_stream.hpp>
#include <nabto/nabto_device_webrtc.hpp>

#include <rtc/global.hpp>

#include <cxxopts/cxxopts.hpp>
#include <nlohmann/json.hpp>
#include <signal.h>

using nlohmann::json;

const char* coapVideoPath[] = { "webrtc", "video", "frontdoor", NULL };

const char* coapTracksPath[] = { "webrtc", "tracks", NULL };

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
    std::vector<nabto::MediaStreamPtr> medias;
    nabto::RtspStreamPtr rtsp = nullptr;
    nabto::H264CodecMatcher rtpVideoCodec;
    nabto::OpusCodecMatcher rtpAudioCodec;
    try {
        std::string rtspUrl = opts["rtspUrl"].get<std::string>();
        rtsp = nabto::RtspStream::create("frontdoor", rtspUrl);
        rtsp->setCodecMatchers(&rtpVideoCodec, &rtpAudioCodec);
        medias.push_back(rtsp);
    } catch (std::exception& ex) {
        // rtspUrl was not set, default to RTP.
        uint16_t port = opts["rtpPort"].get<uint16_t>();

        auto rtpVideo = nabto::RtpClient::create("frontdoor-video");
        rtpVideo->setPort(port);
        rtpVideo->setRtpCodecMatcher(&rtpVideoCodec);
        // Remote host is only used for 2-way medias, video is only 1-way
        // rtpVideo->setRemoteHost("127.0.0.1");

        medias.push_back(rtpVideo);

        auto rtpAudio = nabto::RtpClient::create("frontdoor-audio");
        rtpAudio->setPort(port+1);
        rtpAudio->setRtpCodecMatcher(&rtpAudioCodec);
        rtpAudio->setRemoteHost("127.0.0.1");

        medias.push_back(rtpAudio);
    }

    std::cout << "medias size: " << medias.size() << std::endl;

    auto webrtc = nabto::NabtoDeviceWebrtc::create(eventQueue, device->getDevice());
    webrtc->setCheckAccessCallback([device](NabtoDeviceConnectionRef ref, std::string action) -> bool {
        return nm_iam_check_access(device->getIam(), ref, action.c_str(), NULL);
    });

    webrtc->setTrackEventCallback([device, medias, rtsp](NabtoDeviceConnectionRef connRef, nabto::MediaTrackPtr track) {
        std::cout << "Track event for track: " << track->getTrackId() << std::endl;
        auto feed = track->getTrackId();
        for (auto m : medias) {
            if (m->isTrack(feed)) {
                if (!m->matchMedia(track)) {
                    std::cout << "Codec mismatch for track: " << feed << std::endl << "   THIS FEED WILL NOT WORK" << std::endl << std::endl;
                }
                NabtoDeviceConnectionRef ref = connRef;
                m->addConnection(ref, track);
                track->setCloseCallback([m, ref]() {
                    m->removeConnection(ref);
                    });
            }
            else {
                std::cout << "    media " << feed << " was not a match" << std::endl;
            }
        }
    });

    auto coapTracksListener = example::NabtoDeviceCoapListener::create(device, NABTO_DEVICE_COAP_POST, coapTracksPath, eventQueue);

    coapTracksListener->setCoapCallback([webrtc, device, medias](NabtoDeviceCoapRequest* coap) {
        std::cout << "POST /webrtc/tracks" << std::endl;

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
                        std::cout << "checking feed: " << feed << std::endl;
                        if (m->isTrack(feed)) {
                            found = true;
                            auto sdp = m->sdp();
                            auto media = nabto::MediaTrack::create(feed, sdp);
                            media->setCloseCallback([m, ref]() {
                                m->removeConnection(ref);
                                });
                            m->addConnection(ref, media);
                            list.push_back(media);
                            break;
                        }
                    }
                }
                if (!webrtc->connectionAddMedias(ref, list)) {
                    std::cout << "Failed to add medias to connection" << std::endl;
                    nabto_device_coap_error_response(coap, 500, "Internal Server Error");
                    nabto_device_coap_request_free(coap);
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
        }
        else {
            std::cout << "Got CoAP video stream request but IAM rejected it" << std::endl;
            nabto_device_coap_error_response(coap, 401, NULL);
        }

        std::cout << "Freeing coap request" << std::endl;
        nabto_device_coap_request_free(coap);

        });



    // TODO: remove this legacy coap listener
    auto coapVideoListener = example::NabtoDeviceCoapListener::create(device, NABTO_DEVICE_COAP_GET, coapVideoPath, eventQueue);

    coapVideoListener->setCoapCallback([webrtc, device, medias](NabtoDeviceCoapRequest* coap) {
        std::cout << "Got new coap request" << std::endl;

        NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

        if (nm_iam_check_access(device->getIam(), ref, "Webrtc:VideoStream", NULL)) {
            std::cout << "Handling legacy video coap request" << std::endl;

            bool found = false;
            std::vector<nabto::MediaTrackPtr> list;
            // TODO: remove t1, use t<1/2> as trackId based on isTrack, and remove getTrackId() from mediaStream
            std::string t1 = "frontdoor";
            std::string t2 = "frontdoor-video";
            std::string t3 = "frontdoor-audio";

            for (auto m : medias) {
                if (m->isTrack(t1) ||
                    m->isTrack(t2) ||
                    m->isTrack(t3) ) {
                    found = true;
                    auto sdp = m->sdp();
                    auto feed = m->getTrackId();
                    auto media = nabto::MediaTrack::create(feed, sdp);
                    media->setCloseCallback([m, ref]() {
                        m->removeConnection(ref);
                        });
                    m->addConnection(ref, media);
                    list.push_back(media);
                }
            }
            if (!webrtc->connectionAddMedias(ref, list)) {
                std::cout << "Failed to add medias to connection" << std::endl;
                nabto_device_coap_error_response(coap, 500, "Internal Server Error");
            } else if (found) {
                std::cout << "found track ID in feeds returning 201" << std::endl;
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


