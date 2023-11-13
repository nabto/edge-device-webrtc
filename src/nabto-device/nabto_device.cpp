#include "nabto_device.hpp"

#include "oauth_validator.hpp"

#include <nabto/nabto_device_experimental.h>
#include <nabto/nabto_device_virtual.h>
#include <modules/iam/nm_iam_serializer.h>

namespace nabto {

const char* coapOauthPath[] = { "webrtc", "oauth", NULL };
const char* coapChallengePath[] = { "webrtc", "challenge", NULL };

NabtoDeviceImplPtr NabtoDeviceImpl::create(nlohmann::json& opts, EventQueuePtr queue)
{
    auto ptr = std::make_shared<NabtoDeviceImpl>(queue);
    if(ptr->init(opts)){
        return ptr;
    }
    return nullptr;
}

NabtoDeviceImpl::NabtoDeviceImpl(EventQueuePtr queue) : evQueue_(queue), fileBuffer_(1024, 0)
{
}

NabtoDeviceImpl::~NabtoDeviceImpl()
{
    nm_iam_deinit(&iam_);
    fileStreamListener_.reset();
    if (fileStreamFut_ != NULL) {
        nabto_device_future_free(fileStreamFut_);
    }
    coapOauthListener_.reset();
    coapChallengeListener_.reset();
    nabto_device_free(device_);
}

bool NabtoDeviceImpl::init(nlohmann::json& opts)
{

    try {
        productId_ = opts["productId"].get<std::string>();
        deviceId_ = opts["deviceId"].get<std::string>();
        rawPrivateKey_ = opts["rawPrivateKey"].get<std::string>();
    } catch (std::exception& e ) {
        std::cout << "Missing input option. Options must include: productId, deviceId, rawPrivateKey" << std::endl;
        return false;
    }

    try {
        serverUrl_ = opts["serverUrl"].get<std::string>();
    } catch (std::exception& e) {
        // ignore missing optional option
    }

    try {
        frontendUrl_ = opts["frontendUrl"].get<std::string>();
    } catch (std::exception& e) {
        // ignore missing optional option
    }

    try {
        iamReset_ = opts["iamReset"].get<bool>();
    } catch (std::exception& e) {
        // ignore missing optional option
    }

    try {
        logLevel_ = opts["logLevel"].get<std::string>();
        if (logLevel_ == "trace") {
            iamLogLevel_ = NN_LOG_SEVERITY_TRACE;
        }
        else if (logLevel_ == "warn") {
            iamLogLevel_ = NN_LOG_SEVERITY_WARN;
        }
        else if (logLevel_ == "info") {
            iamLogLevel_ = NN_LOG_SEVERITY_INFO;
        }
        else if (logLevel_ == "error") {
            iamLogLevel_ = NN_LOG_SEVERITY_ERROR;
        }
    } catch (std::exception& e) {
        // ignore missing optional option
    }

    NabtoDeviceError ec;
    if (nabto_device_set_log_std_out_callback(NULL) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_log_level(NULL, logLevel_.c_str()) != NABTO_DEVICE_EC_OK)
    {
        std::cout << "failed to set loglevel or logger" << std::endl;
        return false;
    }

    if ((device_ = nabto_device_new()) == NULL) {
        std::cout << "Failed to create device" << std::endl;
        return false;

    }

    iamLog_.logPrint = &iamLogger;
    iamLog_.userData = this;

    if (!nm_iam_init(&iam_, device_, &iamLog_)) {
        std::cout << "Failed to initialize IAM module" << std::endl;
        return false;
    }
    return true;
}

bool NabtoDeviceImpl::start()
{
    NabtoDeviceError ec;
    char* fp;

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

    if (!setupIam(fp)) {
        std::cout << "Failed to initialize IAM module" << std::endl;
        return false;
    }

    std::cout << "Device: " << productId_ << "." << deviceId_ << " with fingerprint: [" << fp << "]" << std::endl;;
    nabto_device_string_free(fp);

    if (nabto_device_set_product_id(device_, productId_.c_str()) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_device_id(device_, deviceId_.c_str()) != NABTO_DEVICE_EC_OK ||
        nabto_device_enable_mdns(device_) != NABTO_DEVICE_EC_OK)
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

    auto self = shared_from_this();
    coapOauthListener_ = NabtoDeviceCoapListener::create(self, NABTO_DEVICE_COAP_POST, coapOauthPath, evQueue_);

    coapOauthListener_->setCoapCallback([self](NabtoDeviceCoapRequest* coap) {
        self->handleOauthRequest(coap);
    });

    coapChallengeListener_ = NabtoDeviceCoapListener::create(self, NABTO_DEVICE_COAP_POST, coapChallengePath, evQueue_);

    coapChallengeListener_->setCoapCallback([self](NabtoDeviceCoapRequest* coap) {
        self->handleChallengeRequest(coap);
    });

    return setupFileStream();
}

bool NabtoDeviceImpl::resetIam()
{
    std::cout << "Resetting IAM to default configuration and state" << std::endl;
    if (!createDefaultIamConfig()) {
        std::cout << "Failed to create IAM config file" << std::endl;
        return false;
    }
    if (!createDefaultIamState()) {
        std::cout << "Failed to create IAM state file" << std::endl;
        return false;
    }
    std::cout << "Reset successfull" << std::endl;
    return true;
}

bool NabtoDeviceImpl::setupIam(const char* fp)
{
    try {
        auto configFile = std::ifstream(iamConfPath_);
        if (iamReset_ || !configFile.good()) {
            // file does not exist
            std::cout << "IAM was reset or config file does not exist at: " << iamConfPath_ << std::endl << "  Creating one with default values" << std::endl;
            if (!createDefaultIamConfig()) {
                std::cout << "Failed to create IAM config file" << std::endl;
                return false;
            }
            configFile = std::ifstream(iamConfPath_);
        }
        auto iamConf = nlohmann::json::parse(configFile);
        std::string confStr = iamConf.dump();
        struct nm_iam_configuration* conf = nm_iam_configuration_new();
        nm_iam_serializer_configuration_load_json(conf, confStr.c_str(), NULL);
        if (!nm_iam_load_configuration(&iam_, conf)) {
            std::cout << "Failed to load IAM configuration" << std::endl;
            nm_iam_configuration_free(conf);
            return false;
        }
    }
    catch (std::exception& ex) {
        std::cout << "Failed to load IAM config" << std::endl;
        return false;
    }

    try {
        auto stateFile = std::ifstream(iamStatePath_);
        if (iamReset_ || !stateFile.good()) {
            // file does not exist
            std::cout << "IAM was reset or state file does not exist at: " << iamStatePath_ << std::endl << "  Creating one with default values" << std::endl;
            if (!createDefaultIamState()) {
                std::cout << "Failed to create IAM state file" << std::endl;
                return false;
            }
            stateFile = std::ifstream(iamStatePath_);
        }
        auto iamState = nlohmann::json::parse(stateFile);
        std::string stateStr = iamState.dump();
        struct nm_iam_state* state = nm_iam_state_new();
        nm_iam_serializer_state_load_json(state, stateStr.c_str(), NULL);

        try {
            auto initialUsername = iamState["InitialPairingUsername"].get<std::string>();
            auto user = nm_iam_state_find_user_by_username(state, initialUsername.c_str());
            if (user && (user->fingerprint == NULL && user->oauthSubject == NULL)) {
                // We have an initial user and it is unpaired
                // Creating invite link
                std::cout << "################################################################" << std::endl << "# Initial user pairing link:    " << std::endl << "# " << frontendUrl_ << "?p=" << productId_ << "&d=" << deviceId_ << "&u=" << initialUsername;

                if (user->password != NULL) {
                    std::cout << "&pwd=" << user->password;
                }

                if (user->sct != NULL) {
                  std::cout << "&sct=" << user->sct;
                 }

                 std::cout << "&fp=" << fp << std::endl << "################################################################" << std::endl;
            }
        } catch (std::exception& ex) {
            // Ignore
        }

        if (!nm_iam_load_state(&iam_, state)) {
            std::cout << "Failed to load IAM state" << std::endl;
            nm_iam_state_free(state);
            return false;
        }

    }
    catch (std::exception& ex) {
        std::cout << "Failed to load IAM state" << ex.what() << std::endl;
        return false;
    }

    nm_iam_set_state_changed_callback(&iam_, iamStateChanged, this);

    return true;
}

void NabtoDeviceImpl::iamStateChanged(struct nm_iam* iam, void* userdata)
{
    std::cout << "IAM state changed callback" << std::endl;
    NabtoDeviceImpl* self = (NabtoDeviceImpl*)userdata;
    char* stateCStr;
    if (nm_iam_serializer_state_dump_json(nm_iam_dump_state(iam), &stateCStr)) {
        std::string state(stateCStr);
        std::cout << "    Writing state: " << state << std::endl;
        std::ofstream stateFile(self->iamStatePath_);
        stateFile << state;
    }
}

bool NabtoDeviceImpl::setupFileStream()
{
    fileStreamFut_ = nabto_device_future_new(device_);
    auto self = shared_from_this();
    fileStreamListener_ = NabtoDeviceStreamListener::create(self);

    fileStreamListener_->setStreamCallback([self](NabtoDeviceStream* stream) {
        // TODO: split into seperate stream class so we can have multiple file streams in parallel.
        NabtoDeviceConnectionRef ref = nabto_device_stream_get_connection_ref(stream);
        if (self->fileStream_ == NULL && nm_iam_check_access(&self->iam_,ref, "Webrtc:FileStream", NULL))
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
            std::cout << "FileStream opened, but " << (self->fileStream_ == NULL ? "IAM rejected it" : "another is already opened") << std::endl;
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

void NabtoDeviceImpl::handleOauthRequest(NabtoDeviceCoapRequest* coap) {
    uint16_t cf; // expect content format text/plain: cf == 0
    NabtoDeviceError ec;
    if ((ec = nabto_device_coap_request_get_content_format(coap, &cf)) != NABTO_DEVICE_EC_OK || cf != 0) {
        std::cout << "  Invalid content format: " << cf << " ec: " << nabto_device_error_get_message(ec) << std::endl;
        nabto_device_coap_error_response(coap, 400, "invalid content format");
        nabto_device_coap_request_free(coap);
        return;
    }

    char* payload;
    size_t payloadLen;

    if (nabto_device_coap_request_get_payload(coap, (void**)&payload, &payloadLen) != NABTO_DEVICE_EC_OK) {
        nabto_device_coap_error_response(coap, 400, "missing payload");
        nabto_device_coap_request_free(coap);
        return;

    }

    std::string token(payload, payloadLen);

    auto self = shared_from_this();

    NabtoOauthValidatorPtr oauth = std::make_shared<NabtoOauthValidator>(jwksUrl_, jwksIssuer_, productId_, deviceId_);

    oauth->validateToken(token, [self, coap](bool valid, std::string subject) {
        if (valid) {
            NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

            nm_iam_state* state = nm_iam_dump_state(&self->iam_);
            nm_iam_user* user = nm_iam_state_find_user_by_oauth_subject(state, subject.c_str());

            if (user &&
                nm_iam_authorize_connection(&self->iam_, ref, user->username) == NM_IAM_ERROR_OK) {
                nabto_device_coap_response_set_code(coap, 201);
                nabto_device_coap_response_ready(coap);
            } else {
                nabto_device_coap_error_response(coap, 404, "no such user");
            }
            nm_iam_state_free(state);
        } else {
            nabto_device_coap_error_response(coap, 401, "Invalid token");
        }
        nabto_device_coap_request_free(coap);
    });

}


void NabtoDeviceImpl::handleChallengeRequest(NabtoDeviceCoapRequest* coap) {
    uint16_t cf; // expect content format application/json: cf == 50
    NabtoDeviceError ec;
    if ((ec = nabto_device_coap_request_get_content_format(coap, &cf)) != NABTO_DEVICE_EC_OK || cf != NABTO_DEVICE_COAP_CONTENT_FORMAT_APPLICATION_JSON) {
        std::cout << "  Invalid content format: " << cf << " ec: " << nabto_device_error_get_message(ec) << std::endl;
        nabto_device_coap_error_response(coap, 400, "invalid content format");
        nabto_device_coap_request_free(coap);
        return;
    }

    char* payload;
    size_t payloadLen;

    if (nabto_device_coap_request_get_payload(coap, (void**)&payload, &payloadLen) != NABTO_DEVICE_EC_OK) {
        nabto_device_coap_error_response(coap, 400, "missing payload");
        nabto_device_coap_request_free(coap);
        return;
    }

    std::string payloadStr(payload, payloadLen);
    std::string nonce;
    try {
        nlohmann::json challenge = nlohmann::json::parse(payloadStr);
        nonce = challenge["challenge"].get<std::string>();
    } catch (std::exception& e ) {
        std::cout << "Failed to parse json payload: " << payloadStr << std::endl;
        nabto_device_coap_error_response(coap, 400, "Invalid JSON");
        nabto_device_coap_request_free(coap);
        return;
    }

    std::cout << "Got challenge nonce from client: " << nonce << std::endl;

    char* deviceFp = NULL;
    char* clientFp = NULL;

    NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

    if (nabto_device_connection_get_client_fingerprint(device_, ref, &clientFp) != NABTO_DEVICE_EC_OK) {
        std::cout << "Failed to get client fingerprint" << std::endl;
        nabto_device_coap_error_response(coap, 400, "Invalid Connection");
        nabto_device_coap_request_free(coap);
        return;
    }

    if (nabto_device_connection_get_device_fingerprint(device_, ref, &deviceFp) != NABTO_DEVICE_EC_OK) {
        nabto_device_string_free(clientFp);
        std::cout << "Failed to get device fingerprint" << std::endl;
        nabto_device_coap_error_response(coap, 400, "Invalid Connection");
        nabto_device_coap_request_free(coap);
        return;
    }

    // TODO: dont require jwksurl, issuer, product id, device id for this
    NabtoOauthValidatorPtr oauth = std::make_shared<NabtoOauthValidator>(jwksUrl_, jwksIssuer_, productId_, deviceId_);

    std::string devFp(deviceFp, 64);
    std::string cliFp(clientFp, 64);

    auto token = oauth->createChallengeResponse(rawPrivateKey_, cliFp, devFp, nonce);

    nlohmann::json resp = {{"response", token}};
    std::cout << "Sending info response: " << resp.dump() << std::endl;
    auto respPayload = resp.dump(); //nlohmann::json::to_cbor(resp);
    nabto_device_coap_response_set_code(coap, 205);
    nabto_device_coap_response_set_content_format(coap, NABTO_DEVICE_COAP_CONTENT_FORMAT_APPLICATION_JSON);
    nabto_device_coap_response_set_payload(coap, respPayload.data(), respPayload.size());
    nabto_device_coap_response_ready(coap);
    nabto_device_coap_request_free(coap);
    nabto_device_string_free(clientFp);
    nabto_device_string_free(deviceFp);
}

void NabtoDeviceImpl::iamLogger(void* data, enum nn_log_severity severity, const char* module,
    const char* file, int line,
    const char* fmt, va_list args)
{
    NabtoDeviceImpl* self = (NabtoDeviceImpl*)data;
    if (severity <= self->iamLogLevel_) {
        char log[256];
        int ret;

        ret = vsnprintf(log, 256, fmt, args);
        if (ret >= 256) {
            // TODO: handle too long log lines
            // The log line was too large for the array
        }
        size_t fileLen = strlen(file);
        char fileTmp[16 + 4];
        if (fileLen > 16) {
            strcpy(fileTmp, "...");
            strcpy(fileTmp + 3, file + fileLen - 16);
        }
        else {
            strcpy(fileTmp, file);
        }
        const char* level;
        switch (severity) {
        case NN_LOG_SEVERITY_ERROR:
            level = "ERROR";
            break;
        case NN_LOG_SEVERITY_WARN:
            level = "_WARN";
            break;
        case NN_LOG_SEVERITY_INFO:
            level = "_INFO";
            break;
        case NN_LOG_SEVERITY_TRACE:
            level = "TRACE";
            break;
        default:
            // should not happen as it would be caugth by the if
            level = "_NONE";
            break;
        }

        printf("%s(%03u)[%s] %s\n",
            fileTmp, line, level, log);

    }
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


NabtoDeviceCoapListenerPtr NabtoDeviceCoapListener::create(NabtoDeviceImplPtr device, NabtoDeviceCoapMethod method, const char** path, EventQueuePtr queue)
{
    auto ptr = std::make_shared<NabtoDeviceCoapListener>(device, queue);
    if (ptr->start(method, path)) {
        return ptr;
    }
    return nullptr;

}

NabtoDeviceCoapListener::NabtoDeviceCoapListener(NabtoDeviceImplPtr device, EventQueuePtr queue) : device_(device), queue_(queue)
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
        self->queue_->post([self]() {
            self->me_ = nullptr;
            self->coapCb_ = nullptr;
            self->device_ = nullptr;
        });
        return;
    }
    std::function<void(NabtoDeviceCoapRequest* coap)> cb = self->coapCb_;
    NabtoDeviceCoapRequest* req = self->coap_;
    self->queue_->post([cb, req]() {
        cb(req);
    });
    self->coap_ = NULL;
    self->nextCoapRequest();
}



} // Namespace
