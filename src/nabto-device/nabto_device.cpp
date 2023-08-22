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

NabtoDeviceImpl::NabtoDeviceImpl() : fileBuffer_(1024, 0)
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
    if (nabto_device_set_log_std_out_callback(NULL) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_log_level(NULL, logLevel_.c_str()) != NABTO_DEVICE_EC_OK)
    {
        std::cout << "failed to set loglevel or logger" << std::endl;
        return false;
    }
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
    return setupFileStream();
}


bool NabtoDeviceImpl::setupFileStream()
{
    // TODO: ephemeral port number
    if ((fileStreamListen_ = nabto_device_listener_new(device_)) == NULL ||
        nabto_device_stream_init_listener(device_, fileStreamListen_, 655) != NABTO_DEVICE_EC_OK ||
        (fileStreamFut_ = nabto_device_future_new(device_)) == NULL)
    {
        std::cout << "Failed to listen for streams" << std::endl;
        return false;
    }
    nextFileStream();
    return true;
}

void NabtoDeviceImpl::nextFileStream()
{
    nabto_device_listener_new_stream(fileStreamListen_, fileStreamFut_, &fileStream_);
    nabto_device_future_set_callback(fileStreamFut_, newFileStream, this);
}

void NabtoDeviceImpl::newFileStream(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    NabtoDeviceImpl* self = (NabtoDeviceImpl*)userData;
    if (ec != NABTO_DEVICE_EC_OK)
    {
        std::cout << "stream future wait failed: " << nabto_device_error_get_message(ec) << std::endl;
        return;
    }
    // TODO: check IAM
    if (true)
    {
        // TODO: split into seperate stream class so we can have multiple file streams in parallel.
        std::cout << "Got new file stream" << std::endl;
        nabto_device_stream_accept(self->fileStream_, future);
        nabto_device_future_set_callback(future, fileStreamAccepted, self);
    }
    else {
        nabto_device_stream_free(self->fileStream_);
        self->fileStream_ = NULL;
        self->nextFileStream();
    }

}

void NabtoDeviceImpl::fileStreamAccepted(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    NabtoDeviceImpl* self = (NabtoDeviceImpl*)userData;
    if (ec != NABTO_DEVICE_EC_OK) {
        std::cout << "file stream accept failed" << std::endl;
        return;
    }
    std::cout << "File stream accepted" << std::endl;
    self->inputFile_ = std::ifstream("nabto.png", std::ifstream::binary);
    self->doStreamFile();
}

void NabtoDeviceImpl::doStreamFile()
{
    if (!inputFile_.eof()) {
        std::cout << "Input not EOF, reading file" << std::endl;
        inputFile_.read(fileBuffer_.data(), fileBuffer_.size());
        std::streamsize s = inputFile_.gcount();
        std::cout << "Read " << s << "bytes, writing to nabto stream" << std::endl;
        nabto_device_stream_write(fileStream_, fileStreamFut_, fileBuffer_.data(), s);
        nabto_device_future_set_callback(fileStreamFut_, writeFileStreamCb, this);
        return;
    } else {
        std::cout << "File reached EOF, closing nabto stream" << std::endl;
        nabto_device_stream_close(fileStream_, fileStreamFut_);
        nabto_device_future_set_callback(fileStreamFut_, closeFileStreamCb, this);
    }

}

void NabtoDeviceImpl::writeFileStreamCb(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    NabtoDeviceImpl* self = (NabtoDeviceImpl*)userData;
    if (ec != NABTO_DEVICE_EC_OK) {
        std::cout << "file stream write failed" << std::endl;
        return;
    }
    std::cout << "nabto stream write callback" << std::endl;
    self->doStreamFile();

}

void NabtoDeviceImpl::closeFileStreamCb(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    NabtoDeviceImpl* self = (NabtoDeviceImpl*)userData;
    if (ec != NABTO_DEVICE_EC_OK) {
        std::cout << "file stream close failed" << std::endl;
        return;
    }
    std::cout << "Stream closed" << std::endl;
    nabto_device_stream_abort(self->fileStream_);
    // We never read, we are not writing, we are done closeing, so we do not have unresolved futures.
    nabto_device_stream_free(self->fileStream_);
    self->nextFileStream();
}

void NabtoDeviceImpl::stop() {
    return nabto_device_stop(device_);
}

} // Namespace
