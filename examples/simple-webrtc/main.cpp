#include "coap_listener.hpp"

#include <nabto/nabto_device_webrtc.hpp>

#include <event-queue/event_queue_impl.hpp>
#include <media-streams/rtp_client.hpp>

#include <rtc/global.hpp>
#include <nlohmann/json.hpp>
#include <signal.h>

using nlohmann::json;

const char* coapPath[] = { "webrtc", "get", NULL };

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


bool initDevice(nabto::NabtoDevicePtr device, json& opts);

int main(int argc, char** argv) {

    json opts;
    opts["productId"] = std::string("pr-jzoqqta9");
    opts["deviceId"] = std::string("de-uguiictr");
    opts["sct"] = std::string("demosct");
    opts["rawPrivateKey"] = std::string("5265f6ecbe263d51fc0ceacf6e6555840435150318ad5e24329cba6563a82025");
    opts["rtpPort"] = 6000;
    opts["logLevel"] = std::string("info");

    auto device = nabto::makeNabtoDevice();

    if (!initDevice(device, opts)){
        return 1;
    }

    nabto::H264CodecMatcher rtpVideoCodec;
    uint16_t port = opts["rtpPort"].get<uint16_t>();
    auto rtpVideo = nabto::RtpClient::create("frontdoor-video");
    rtpVideo->setPort(port);
    rtpVideo->setRtpCodecMatcher(&rtpVideoCodec);

    auto eventQueue = nabto::EventQueueImpl::create();

    auto coapListener = CoapListener::create(device, NABTO_DEVICE_COAP_GET, coapPath, eventQueue);

    auto webrtc = nabto::NabtoDeviceWebrtc::create(eventQueue, device);
    webrtc->setCheckAccessCallback([](NabtoDeviceConnectionRef ref, std::string action) -> bool {
        return true;
        });

    coapListener->setCoapCallback([webrtc, device, rtpVideo, &rtpVideoCodec](NabtoDeviceCoapRequest* coap) {
        std::cout << "Got new coap request" << std::endl;

        NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

        std::vector<nabto::MediaTrackPtr> list;

        auto trackId = rtpVideo->getTrackId();
        auto sdp = rtpVideo->sdp();
        auto media = nabto::MediaTrack::create(trackId, sdp);
        media->setCloseCallback([rtpVideo, ref]() {
            rtpVideo->removeConnection(ref);
        });
        const rtc::SSRC ssrc = rtpVideoCodec.ssrc();
        nabto::RtpTrack track = {
            media,
            ssrc,
            rtpVideoCodec.payloadType(),
            rtpVideoCodec.payloadType()
        };
        rtpVideo->addConnection(ref, track);

        list.push_back(media);

        if (!webrtc->connectionAddMedias(ref, list)) {
            std::cout << "Failed to add medias to connection" << std::endl;
            nabto_device_coap_error_response(coap, 500, "Internal Server Error");
            return;
        }

        nabto_device_coap_response_set_code(coap, 201);
        nabto_device_coap_response_ready(coap);

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

    std::cout << "Device: " << opts["productId"].get<std::string>() << "." << opts["deviceId"].get<std::string>() << " with fingerprint: [" << fp << "]" << std::endl;;
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
