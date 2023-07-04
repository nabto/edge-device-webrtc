#include "nabto_device.hpp"
#include <nabto/nabto_device_experimental.h>

namespace nabto {

NabtoDeviceImplPtr NabtoDeviceImpl::create(nlohmann::json& opts)
{
    auto ptr = std::make_shared<NabtoDeviceImpl>();
    if(ptr->init(opts)){
        return ptr;
    }
    return nullptr;
}

NabtoDeviceImpl::NabtoDeviceImpl()
{

}

NabtoDeviceImpl::~NabtoDeviceImpl()
{
    nabto_device_free(device_);
}

bool NabtoDeviceImpl::init(nlohmann::json& opts)
{

    try {
        this->productId_ = opts["productId"].get<std::string>();
        this->deviceId_ = opts["deviceId"].get<std::string>();
        this->rawPrivateKey_ = opts["rawPrivateKey"].get<std::string>();
    } catch (std::exception& e ) {
        std::cout << "Missing input option. Options must include: productId, deviceId, rawPrivateKey" << std::endl;
        return false;
    }

    try {
        this->serverUrl_ = opts["serverUrl"].get<std::string>();
    } catch (std::exception& e) {
        // ignore missing optional option
    }

    try {
        this->sct_ = opts["sct"].get<std::string>();
    } catch (std::exception& e) {
        // ignore missing optional option
    }

    try {
        this->logLevel_ = opts["logLevel"].get<std::string>();
    } catch (std::exception& e) {
        // ignore missing optional option
    }
    return true;
}

bool NabtoDeviceImpl::start()
{
    NabtoDeviceError ec;
    char* fp;
    if ((device_ = nabto_device_new()) == NULL) {
        std::cout << "Failed to create device" << std::endl;
        return false;

    }

    uint8_t key[32];

    for (size_t i = 0; i < 32; i++) {
        std::string s(&rawPrivateKey_[i * 2], 2);
        key[i] = std::stoi(s, 0, 16);
    }

    if ((ec = nabto_device_set_private_key_secp256r1(device_, key, 32)) != NABTO_DEVICE_EC_OK) {
        std::cout << "Failed to set private key, ec=" << nabto_device_error_get_message(ec) << std::endl;
        return false;
    }

    if (nabto_device_get_device_fingerprint(device_, &fp) != NABTO_DEVICE_EC_OK) {
        return false;
    }

    std::cout << "Device: " << productId_ << "." << deviceId_ << " with fingerprint: [" << fp << "]" << std::endl;;
    nabto_device_string_free(fp);

    if (nabto_device_set_product_id(device_, productId_.c_str()) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_device_id(device_, deviceId_.c_str()) != NABTO_DEVICE_EC_OK ||
        nabto_device_enable_mdns(device_) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_log_std_out_callback(device_) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_log_level(NULL, logLevel_.c_str()) != NABTO_DEVICE_EC_OK ||
        nabto_device_add_server_connect_token(device_, sct_.c_str()) != NABTO_DEVICE_EC_OK)
    {
        return false;
    }

    if (!serverUrl_.empty() && nabto_device_set_server_url(device_, serverUrl_.c_str()) != NABTO_DEVICE_EC_OK) {
        return false;
    }

    NabtoDeviceFuture* fut = nabto_device_future_new(device_);
    nabto_device_start(device_, fut);

    ec = nabto_device_future_wait(fut);
    nabto_device_future_free(fut);
    if (ec != NABTO_DEVICE_EC_OK) {
        std::cout << "Failed to start device, ec=" << nabto_device_error_get_message(ec) << std::endl;
        return false;
    }
    return true;


}


void NabtoDeviceImpl::stop() {
    return nabto_device_stop(device_);
}

} // Namespace
