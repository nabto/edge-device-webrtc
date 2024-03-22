#pragma once
#include <nabto/nabto_device.h>
#include <nabto/nabto_device_experimental.h>

#include <curl/curl.h>

#include <signal.h>
#include <future>
#include <random>
#include <iostream>
#include <iomanip> // For std::setfill and std::setw

namespace nabto {

class CurlAsync;
typedef std::shared_ptr<CurlAsync> CurlAsyncPtr;

class CurlAsync : public std::enable_shared_from_this<CurlAsync> {
public:
    static CurlAsyncPtr create();
    CurlAsync();
    ~CurlAsync();

    bool init();
    void stop();

    // Get the curl reference to build your request on using `curl_easy_XX()` functions
    CURL* getCurl() { return curl_; }

    // Invokes the request. This starts a new thread in which the request is invoked synchroneusly. The callback is invoked from the created thread once the request is resolved.
    // Returns false if a thread is already running
    bool asyncInvoke(std::function<void(CURLcode res, uint16_t statusCode)> callback);

    // Reinvoke the request. This must be called from the callback of a previous request. This request will reuse the std::thread created for the first request. getCurl() can be used to build a new request in the first callback before calling this.
    void asyncReinvoke(std::function<void(CURLcode res, uint16_t statusCode)> callback);

    // Reinvoke the request. This must be called from the callback of a previous request and is a direct blocking invocation of the curl_easy_perform.
    CURLcode reinvoke();
    void reinvokeStatus(CURLcode* code, uint16_t* status);


private:
    static void threadRunner(CurlAsync*  self);

    CURL* curl_;
    std::thread thread_;
    std::mutex mutex_;
    std::function<void(CURLcode res, uint16_t statusCode)> callback_;
    bool reinvoke_ = false;
    bool stopped_ = false;
    CurlAsyncPtr me_ = nullptr;

};

class PrivateKeyCreator {
public:
    PrivateKeyCreator() {
        createPrivateKey();
    }

    ~PrivateKeyCreator() {
        if (fp_) {
            nabto_device_string_free(fp_);
        }
        if (dev_) {
            nabto_device_stop(dev_);
            nabto_device_free(dev_);
        }
    }

private:

    void createPrivateKey()
    {
        NabtoDeviceError ec;
        std::random_device random_device;
        std::mt19937 generator(random_device());
        std::uniform_int_distribution<> distribution(0, 255);

        uint8_t key[32];
        for (size_t i = 0; i < 32; i++) {
            key[i] = distribution(generator);
        }

        dev_ = nabto_device_new();
        if (dev_ == NULL) {
            std::cout << "Failed to create device context" << std::endl;
            return;
        }

        if ((ec = nabto_device_set_private_key_secp256r1(dev_, key, 32)) != NABTO_DEVICE_EC_OK) {
            std::cout << "Failed to set private key, ec=" << nabto_device_error_get_message(ec) << std::endl;
            return;
        }

        if ((ec = nabto_device_get_device_fingerprint(dev_, &fp_)) != NABTO_DEVICE_EC_OK) {
            std::cout << "Failed to get fingerprint, ec=" << nabto_device_error_get_message(ec) << std::endl;
            return;
        }

        std::cout << "Created Raw private key: " << std::endl;
        std::cout << "  ";
        for (int i = 0; i < 32; i++) {
            std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)key[i];
        }
        std::cout << std::dec << std::endl;

        std::cout << "With fingerprint: " << std::endl << "  " << fp_ << std::endl;

        return;
    }


    NabtoDevice* dev_ = NULL;
    char* fp_ = NULL;
};


} // namespace nabto
