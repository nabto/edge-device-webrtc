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
    fileStreamListener_.reset();
    if (fileStreamFut_ != NULL) {
        nabto_device_future_free(fileStreamFut_);
    }
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
    // TODO: remove setupPassword() when IAM implements passwords
    return setupFileStream() && setupPassword();
}

bool NabtoDeviceImpl::setupPassword()
{
    if ((passwordListen_ = nabto_device_listener_new(device_)) == NULL ||
        nabto_device_password_authentication_request_init_listener(device_, passwordListen_) != NABTO_DEVICE_EC_OK ||
        (passwordFut_ = nabto_device_future_new(device_)) == NULL) {
        std::cout << "Failed to listen for password authorization requests" << std::endl;
        return false;
    }
    nextPasswordRequest();
    return true;
}

void NabtoDeviceImpl::nextPasswordRequest()
{
    nabto_device_listener_new_password_authentication_request(passwordListen_, passwordFut_, &passwordReq_);
    nabto_device_future_set_callback(passwordFut_, newPasswordRequest, this);
}

void NabtoDeviceImpl::newPasswordRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    NabtoDeviceImpl* self = (NabtoDeviceImpl*)userData;
    if (ec != NABTO_DEVICE_EC_OK)
    {
        std::cout << "password request wait failed: " << nabto_device_error_get_message(ec) << std::endl;
        nabto_device_future_free(future);
        nabto_device_listener_free(self->passwordListen_);
        return;
    }
    const char* uname = nabto_device_password_authentication_request_get_username(self->passwordReq_);
    std::cout << "Got password request for username: " << std::string(uname) << std::endl;
    if (std::string(uname) == std::string("foo")) {
        std::cout << "    Setting password \"bar\"" << std::endl;
        nabto_device_password_authentication_request_set_password(self->passwordReq_, "bar");
    }
    nabto_device_password_authentication_request_free(self->passwordReq_);
    self->nextPasswordRequest();
}

bool NabtoDeviceImpl::setupFileStream()
{
    fileStreamFut_ = nabto_device_future_new(device_);
    auto self = shared_from_this();
    fileStreamListener_ = NabtoDeviceStreamListener::create(self);

    fileStreamListener_->setStreamCallback([self](NabtoDeviceStream* stream) {
        // TODO: split into seperate stream class so we can have multiple file streams in parallel.
        // TODO: check IAM
        if (self->fileStream_ == NULL && true)
        {
            // if we don't already have an open file stream
            // and IAM allowed the stream
            std::cout << "Got new file stream" << std::endl;
            self->fileStream_ = stream;
            self->me_ = self; // keep self alive until stream is closed
            nabto_device_stream_accept(self->fileStream_, self->fileStreamFut_);
            nabto_device_future_set_callback(self->fileStreamFut_, fileStreamAccepted, self.get());
        }
        else {
            nabto_device_stream_free(stream);
        }

    });
    return true;
}

void NabtoDeviceImpl::fileStreamAccepted(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    NabtoDeviceImpl* self = (NabtoDeviceImpl*)userData;
    if (ec != NABTO_DEVICE_EC_OK) {
        std::cout << "file stream accept failed" << std::endl;
        self->fileStream_ = NULL;
        self->me_ = nullptr;
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
        self->fileStream_ = NULL;
        self->me_ = nullptr;
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
        self->fileStream_ = NULL;
        self->me_ = nullptr;
        return;
    }
    std::cout << "Stream closed" << std::endl;
    nabto_device_stream_abort(self->fileStream_);
    // We never read, we are not writing, we are done closeing, so we do not have unresolved futures.
    nabto_device_stream_free(self->fileStream_);
}

uint32_t NabtoDeviceImpl::getFileStreamPort()
{
    return fileStreamListener_->getStreamPort();
}


void NabtoDeviceImpl::stop() {
    return nabto_device_stop(device_);
}

NabtoDeviceStreamListenerPtr NabtoDeviceStreamListener::create(NabtoDeviceImplPtr device)
{
    auto ptr = std::make_shared<NabtoDeviceStreamListener>(device);
    if (ptr->start()) {
        return ptr;
    }
    return nullptr;
}

NabtoDeviceStreamListener::NabtoDeviceStreamListener(NabtoDeviceImplPtr device) : device_(device)
{
    streamListen_ = nabto_device_listener_new(device_->getDevice());
    streamFut_ = nabto_device_future_new(device_->getDevice());
}

NabtoDeviceStreamListener::~NabtoDeviceStreamListener()
{
    nabto_device_future_free(streamFut_);
    nabto_device_listener_free(streamListen_);
}

bool NabtoDeviceStreamListener::start()
{
    if (streamListen_ == NULL ||
        streamFut_ == NULL ||
        nabto_device_stream_init_listener_ephemeral(device_->getDevice(), streamListen_, &streamPort_) != NABTO_DEVICE_EC_OK)
    {
        std::cout << "Failed to listen for streams" << std::endl;
        return false;
    }
    me_ = shared_from_this();
    nextStream();
    return true;

}

void NabtoDeviceStreamListener::nextStream()
{
    nabto_device_listener_new_stream(streamListen_, streamFut_, &stream_);
    nabto_device_future_set_callback(streamFut_, newStream, this);
}

void NabtoDeviceStreamListener::newStream(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    NabtoDeviceStreamListener* self = (NabtoDeviceStreamListener*)userData;
    if (ec != NABTO_DEVICE_EC_OK)
    {
        std::cout << "stream future wait failed: " << nabto_device_error_get_message(ec) << std::endl;
        self->me_ = nullptr;
        self->streamCb_ = nullptr;
        self->device_ = nullptr;
        return;
    }
    self->streamCb_(self->stream_);
    self->stream_ = NULL;
    self->nextStream();

}


NabtoDeviceCoapListenerPtr NabtoDeviceCoapListener::create(NabtoDeviceImplPtr device, NabtoDeviceCoapMethod method, const char** path)
{
    auto ptr = std::make_shared<NabtoDeviceCoapListener>(device);
    if (ptr->start(method, path)) {
        return ptr;
    }
    return nullptr;

}

NabtoDeviceCoapListener::NabtoDeviceCoapListener(NabtoDeviceImplPtr device) : device_(device)
{
    listener_ = nabto_device_listener_new(device_->getDevice());
    future_ = nabto_device_future_new(device_->getDevice());

}

NabtoDeviceCoapListener::~NabtoDeviceCoapListener()
{
    nabto_device_future_free(future_);
    nabto_device_listener_free(listener_);

}

bool NabtoDeviceCoapListener::start(NabtoDeviceCoapMethod method, const char** path)
{
    if (listener_ == NULL ||
        future_ == NULL ||
        nabto_device_coap_init_listener(device_->getDevice(), listener_, method, path) != NABTO_DEVICE_EC_OK)
    {
        std::cout << "Failed to listen for CoAP requests" << std::endl;
        return false;
    }
    me_ = shared_from_this();
    nextCoapRequest();
    return true;

}

void NabtoDeviceCoapListener::nextCoapRequest()
{
    nabto_device_listener_new_coap_request(listener_, future_, &coap_);
    nabto_device_future_set_callback(future_, newCoapRequest, this);

}

void NabtoDeviceCoapListener::newCoapRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    NabtoDeviceCoapListener* self = (NabtoDeviceCoapListener*)userData;
    if (ec != NABTO_DEVICE_EC_OK)
    {
        std::cout << "Coap listener future wait failed: " << nabto_device_error_get_message(ec) << std::endl;
        self->me_ = nullptr;
        self->coapCb_ = nullptr;
        self->device_ = nullptr;
        return;
    }
    self->coapCb_(self->coap_);
    self->coap_ = NULL;
    self->nextCoapRequest();
}



} // Namespace
