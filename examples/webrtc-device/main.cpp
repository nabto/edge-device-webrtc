#include <webrtc.hpp>
#include <nabto/nabto_device.h>
#include <nabto/nabto_device_experimental.h>
#include "rtsp_client.hpp"

#include <cxxopts/cxxopts.hpp>

#include <signal.h>
#include <sys/socket.h>
typedef int SOCKET;
#include <arpa/inet.h>
#include <netinet/in.h>

const int RTP_BUFFER_SIZE = 2048;


std::string productId = "pr-4nmagfvj";
std::string deviceId = "de-sw9unmnn";
std::string serverUrl = "pr-4nmagfvj.devices.dev.nabto.net";
std::string privateKey = "b5b45deb271a63071924a219a42b0b67146e50f15e2147c9c5b28f7cf9d1015d";
std::string sct = "demosct";
std::string logLevel = "info";
uint16_t rtpPort = 6000;
std::string rtspUrl = "";

bool prepare_device(NabtoDevice* device);
bool start_device(NabtoDevice* dev);
void handle_device_error(NabtoDevice* d, NabtoDeviceListener* l, std::string msg);
void parse_options(int argc, char** argv);

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

int main(int argc, char** argv) {
    parse_options(argc, argv);

    if ((device = nabto_device_new()) == NULL ||
        !prepare_device(device)) {
        handle_device_error(device, NULL, "Failed to prepare device");
        return -1;
    }
    start_device(device);
    nabto::NabtoWebrtc webrtc(device, &check_access, NULL);
    signal(SIGINT, &signal_handler);

    auto rtsp = std::make_shared<nabto::RtspClient>(rtspUrl);
    if (rtspUrl != "" && !rtsp->init()) {
        std::cout << "failed to initialize RTSP client" << std::endl;
        return -1;
    }

    webrtc.start();

    if (rtspUrl != "") {
        rtsp->run([&webrtc](char* buffer, int len) {
            webrtc.handleVideoData((uint8_t*)buffer, len);
            });

    } else {
        SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        addr.sin_port = htons(rtpPort);
        if (bind(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::string err = "Failed to bind UDP socket on 127.0.0.1:" + rtpPort;
            throw std::runtime_error(err);
        }

        int rcvBufSize = 212992;
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvBufSize),
            sizeof(rcvBufSize));
        // Receive from UDP
        char buffer[RTP_BUFFER_SIZE];
        int len;
        int count = 0;
        while ((len = recv(sock, buffer, RTP_BUFFER_SIZE, 0)) >= 0 && !stopped) {
            count++;
            if (count % 100 == 0) {
                std::cout << ".";
            }
            if (count % 1600 == 0) {
                std::cout << std::endl;
                count = 0;
            }
            webrtc.handleVideoData((uint8_t*)buffer, len);
        }
    }

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

    std::cout << "Device: " << productId << "." << deviceId << " with fingerprint: [" << fp << "]" << std::endl;;
    nabto_device_string_free(fp);

    if (nabto_device_set_product_id(dev, productId.c_str()) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_device_id(dev, deviceId.c_str()) != NABTO_DEVICE_EC_OK ||
        nabto_device_enable_mdns(dev) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_log_std_out_callback(dev) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_log_level(NULL, logLevel.c_str()) != NABTO_DEVICE_EC_OK ||
        nabto_device_add_server_connect_token(dev, sct.c_str()) != NABTO_DEVICE_EC_OK)
    {
        return false;
    }

    if (nabto_device_set_server_url(dev, serverUrl.c_str()) != NABTO_DEVICE_EC_OK) {
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


void parse_options(int argc, char** argv)
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
            ("h,help", "Shows this help text");
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help({ "", "Group" }) << std::endl;
            exit(0);
        }

        productId = result["productid"].as<std::string>();
        deviceId = result["deviceid"].as<std::string>();
        serverUrl = result["serverurl"].as<std::string>();
        privateKey = result["privatekey"].as<std::string>();
        sct = result["sct"].as<std::string>();
        logLevel = result["loglevel"].as<std::string>();
        if (result.count("rtsp")) {
            rtspUrl = result["rtsp"].as<std::string>();
        }
        rtpPort = result["rtpport"].as<uint16_t>();

    }
    catch (const cxxopts::OptionException& e)
    {
        std::cout << "Error parsing options: " << e.what() << std::endl;
        exit(1);
    }

}
