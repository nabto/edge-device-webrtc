#include <webrtc.hpp>
#include <nabto/nabto_device.h>
#include <nabto/nabto_device_experimental.h>
#include <signal.h>
#include <sys/socket.h>
#include "rtsp_client.hpp"
typedef int SOCKET;
#include <arpa/inet.h>
#include <netinet/in.h>


const char* productId = "pr-4nmagfvj";
const char* deviceId = "de-sw9unmnn";
const char* serverUrl = "pr-4nmagfvj.devices.dev.nabto.net";
const char* privateKey = "b5b45deb271a63071924a219a42b0b67146e50f15e2147c9c5b28f7cf9d1015d";
const char* sct = "demosct";

bool prepare_device(NabtoDevice* device);
bool start_device(NabtoDevice* dev);
void handle_device_error(NabtoDevice* d, NabtoDeviceListener* l, std::string msg);

NabtoDevice* device;
bool stopped = false;

void signal_handler(int s)
{
    (void)s;
     NabtoDeviceFuture* fut = nabto_device_future_new(device);
    if (fut == NULL) {
        printf("cannot allocate future for nice shutdown\n");
        exit(1);
    }
    nabto_device_close(device, fut); // triggers NABTO_DEVICE_EVENT_CLOSED in event listener
    nabto_device_future_wait(fut);
    nabto_device_future_free(fut);
    nabto_device_stop(device);
    stopped = true;
}

bool check_access(NabtoDeviceConnectionRef ref, const char* action, void* userData) {
    return true;
}

int main() {

    if ((device = nabto_device_new()) == NULL ||
        !prepare_device(device)) {
        handle_device_error(device, NULL, "Failed to prepare device");
        return -1;
    }
    start_device(device);
    nabto::NabtoWebrtc webrtc(device, &check_access, NULL);
    signal(SIGINT, &signal_handler);
    auto rtsp = std::make_shared<nabto::RtspClient>("rtsp://127.0.0.1:8554/video");
    rtsp->init();
    webrtc.start();
    rtsp->run([&webrtc](char* buffer, int len) {
        webrtc.handleVideoData((uint8_t*)buffer, len);
        });

    // SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    // struct sockaddr_in addr = {};
    // addr.sin_family = AF_INET;
    // addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    // addr.sin_port = htons(6000);
    // if (bind(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0)
    //     throw std::runtime_error("Failed to bind UDP socket on 127.0.0.1:6000");

    // int rcvBufSize = 212992;
    // setsockopt(sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvBufSize),
    //     sizeof(rcvBufSize));
    // // Receive from UDP
    // char buffer[RTSP_BUFFER_SIZE];
    // int len;
    // int count = 0;
    // while ((len = recv(sock, buffer, RTSP_BUFFER_SIZE, 0)) >= 0 && !stopped) {
    //     count++;
    //     if (count % 100 == 0) {
    //         std::cout << ".";
    //     }
    //     if (count % 1600 == 0) {
    //         std::cout << std::endl;
    //         count = 0;
    //     }
    //     //            std::cout << "Received packet size: " << len << std::endl;
    //     webrtc.handleVideoData((uint8_t*)buffer, len);
    // }


}

bool prepare_device(NabtoDevice* dev)
{
    NabtoDeviceError ec;
    char* fp;

    uint8_t key[32];

    for (size_t i = 0; i < 32; i++) {
        std::string s(&privateKey[i*2], 2);
        key[i] = std::stoi(s, 0, 16);
    }

    if ((ec = nabto_device_set_private_key_secp256r1(dev, key, 32)) != NABTO_DEVICE_EC_OK) {
        printf("Failed to set private key, ec=%s\n", nabto_device_error_get_message(ec));
        return false;
    }

    if (nabto_device_get_device_fingerprint(dev, &fp) != NABTO_DEVICE_EC_OK) {
        return false;
    }

    printf("Device: %s.%s with fingerprint: [%s]\n", productId, deviceId, fp);
    nabto_device_string_free(fp);

    if (nabto_device_set_product_id(dev, productId) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_device_id(dev, deviceId) != NABTO_DEVICE_EC_OK ||
        nabto_device_enable_mdns(dev) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_log_std_out_callback(dev) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_log_level(NULL, "info") != NABTO_DEVICE_EC_OK ||
        nabto_device_add_server_connect_token(dev, sct) != NABTO_DEVICE_EC_OK)
    {
        return false;
    }

    if (nabto_device_set_server_url(dev, serverUrl) != NABTO_DEVICE_EC_OK) {
        return false;
    }

    return true;
}


bool start_device(NabtoDevice* dev)
{
    NabtoDeviceError ec;
    NabtoDeviceFuture* fut = nabto_device_future_new(dev);
    nabto_device_start(dev, fut);

    ec = nabto_device_future_wait(fut);
    nabto_device_future_free(fut);
    if (ec != NABTO_DEVICE_EC_OK) {
        printf("Failed to start device, ec=%s\n", nabto_device_error_get_message(ec));
        return false;
    }
    return true;
}

void handle_device_error(NabtoDevice* d, NabtoDeviceListener* l, std::string msg)
{
    NabtoDeviceFuture* f = nabto_device_future_new(d);
    if (d) {
        nabto_device_close(d, f);
        nabto_device_future_wait(f);
        nabto_device_stop(d);
        nabto_device_free(d);
    }
    if (f) {
        nabto_device_future_free(f);
    }
    if (l) {
        nabto_device_listener_free(l);
    }
    std::cout << msg << std::endl;
}
