#include "nabto_device.hpp"

#include "oauth_validator.hpp"

#include <nabto/nabto_device_experimental.h>
#include <nabto/nabto_device_virtual.h>
#include <modules/iam/nm_iam_serializer.h>

#if defined(HAVE_FILESYSTEM_H)
#include <filesystem>
#endif

namespace example {

const char* coapOauthPath[] = { "webrtc", "oauth", NULL };
const char* coapChallengePath[] = { "webrtc", "challenge", NULL };

NabtoDeviceAppPtr NabtoDeviceApp::create(nlohmann::json& opts, nabto::EventQueuePtr queue)
{
    auto ptr = std::make_shared<NabtoDeviceApp>(queue);
    if(ptr->init(opts)){
        return ptr;
    }
    return nullptr;
}

NabtoDeviceApp::NabtoDeviceApp(nabto::EventQueuePtr queue) : evQueue_(queue), fileBuffer_(1024, 0), device_(nabto::makeNabtoDevice())
{
}

NabtoDeviceApp::~NabtoDeviceApp()
{
    if (iamLog_.userData != NULL) {
        nm_iam_deinit(&iam_);
    }
    fileStreamListener_.reset();
    if (fileStreamFut_ != NULL) {
        nabto_device_future_free(fileStreamFut_);
    }
    coapOauthListener_.reset();
    coapChallengeListener_.reset();
}

bool NabtoDeviceApp::init(nlohmann::json& opts)
{
    memset(&iamLog_, 0, sizeof(struct nn_log));
    try {
        productId_ = opts["productId"].get<std::string>();
        deviceId_ = opts["deviceId"].get<std::string>();
        rawPrivateKey_ = opts["rawPrivateKey"].get<std::string>();
        if (opts.contains("caBundle")) {
            caBundle_ = opts["caBundle"].get<std::string>();
        }
    } catch (std::exception& e ) {
        NPLOGE << "Missing input option. Options must include: productId, deviceId, rawPrivateKey";
        return false;
    }

    if (rawPrivateKey_.size() != 64) {
        NPLOGE << "Invalid private key length " << rawPrivateKey_.size() << " != 64";
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
        auto homedir = opts["homeDir"].get<std::string>();
        iamConfPath_ = homedir + "/iam_config.json";
        iamStatePath_ = homedir + "/iam_state.json";
        createHomeDir(homedir);

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
        char* envLvl = std::getenv("NABTO_LOG_LEVEL");
        if (envLvl != NULL) {
            logLevel_ = std::string(envLvl);
        }
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
        NPLOGE << "failed to set loglevel or logger";
        return false;
    }

    iamLog_.logPrint = &iamLogger;
    iamLog_.userData = this;

    if (!nm_iam_init(&iam_, device_.get(), &iamLog_)) {
        NPLOGE << "Failed to initialize IAM module";
        return false;
    }
    return true;
}

bool NabtoDeviceApp::start()
{
    NabtoDeviceError ec;
    char* fp;

    uint8_t key[32];

    for (size_t i = 0; i < 32; i++) {
        std::string s(&rawPrivateKey_[i * 2], 2);
        key[i] = std::stoi(s, 0, 16);
    }

    if ((ec = nabto_device_set_private_key_secp256r1(device_.get(), key, 32)) != NABTO_DEVICE_EC_OK) {
        NPLOGE << "Failed to set private key, ec=" << nabto_device_error_get_message(ec);
        return false;
    }

    if (nabto_device_get_device_fingerprint(device_.get(), &fp) != NABTO_DEVICE_EC_OK) {
        return false;
    }

    if (!setupIam(fp)) {
        NPLOGE << "Failed to initialize IAM module";
        return false;
    }

    NPLOGI << "Nabto Embedded SDK version: " << nabto_device_version();

    NPLOGI << "Device: " << productId_ << "." << deviceId_ << " with fingerprint: [" << fp << "]";
    nabto_device_string_free(fp);

    if (nabto_device_set_product_id(device_.get(), productId_.c_str()) != NABTO_DEVICE_EC_OK ||
        nabto_device_set_device_id(device_.get(), deviceId_.c_str()) != NABTO_DEVICE_EC_OK ||
        nabto_device_enable_mdns(device_.get()) != NABTO_DEVICE_EC_OK)
    {
        return false;
    }

    if (!serverUrl_.empty() && nabto_device_set_server_url(device_.get(), serverUrl_.c_str()) != NABTO_DEVICE_EC_OK) {
        return false;
    }

    NabtoDeviceFuture* fut = nabto_device_future_new(device_.get());
    nabto_device_start(device_.get(), fut);

    ec = nabto_device_future_wait(fut);
    nabto_device_future_free(fut);
    if (ec != NABTO_DEVICE_EC_OK) {
        NPLOGE << "Failed to start device, ec=" << nabto_device_error_get_message(ec);
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

bool NabtoDeviceApp::resetIam()
{
    NPLOGI << "Resetting IAM to default configuration and state";
    if (!createDefaultIamConfig()) {
        NPLOGE << "Failed to create IAM config file";
        return false;
    }
    if (!createDefaultIamState()) {
        NPLOGE << "Failed to create IAM state file";
        return false;
    }
    NPLOGI << "Reset successfull";
    return true;
}

bool NabtoDeviceApp::setupIam(const char* fp)
{
    try {
        auto configFile = std::ifstream(iamConfPath_);
        if (iamReset_ || !configFile.good()) {
            // file does not exist
            NPLOGI << "IAM was reset or config file does not exist at: " << iamConfPath_;
            NPLOGI << "  Creating one with default values";
            if (!createDefaultIamConfig()) {
                NPLOGE << "Failed to create IAM config file";
                return false;
            }
            configFile = std::ifstream(iamConfPath_);
        }
        auto iamConf = nlohmann::json::parse(configFile);
        std::string confStr = iamConf.dump();
        struct nm_iam_configuration* conf = nm_iam_configuration_new();
        nm_iam_serializer_configuration_load_json(conf, confStr.c_str(), NULL);
        if (!nm_iam_load_configuration(&iam_, conf)) {
            NPLOGE << "Failed to load IAM configuration";
            nm_iam_configuration_free(conf);
            return false;
        }
    }
    catch (std::exception& ex) {
        NPLOGE << "Failed to load IAM config";
        return false;
    }

    try {
        auto stateFile = std::ifstream(iamStatePath_);
        if (iamReset_ || !stateFile.good()) {
            // file does not exist
            NPLOGI << "IAM was reset or state file does not exist at: " << iamStatePath_;
            NPLOGI << "  Creating one with default values";
            if (!createDefaultIamState()) {
                NPLOGE << "Failed to create IAM state file";
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
            if (user && (nn_llist_empty(&user->fingerprints) && user->oauthSubject == NULL)) {
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
            NPLOGE << "Failed to load IAM state";
            nm_iam_state_free(state);
            return false;
        }

    }
    catch (std::exception& ex) {
        NPLOGE << "Failed to load IAM state" << ex.what();
        return false;
    }

    nm_iam_set_state_changed_callback(&iam_, iamStateChanged, this);

    return true;
}

void NabtoDeviceApp::iamStateChanged(struct nm_iam* iam, void* userdata)
{
    NabtoDeviceApp* self = (NabtoDeviceApp*)userdata;
    char* stateCStr;
    if (nm_iam_serializer_state_dump_json(nm_iam_dump_state(iam), &stateCStr)) {
        std::string state(stateCStr);
        std::ofstream stateFile(self->iamStatePath_);
        stateFile << state;
    }
}

bool NabtoDeviceApp::setupFileStream()
{
    fileStreamFut_ = nabto_device_future_new(device_.get());
    auto self = shared_from_this();
    fileStreamListener_ = NabtoDeviceStreamListener::create(self, evQueue_);

    fileStreamListener_->setStreamCallback([self](NabtoDeviceStream* stream) {
        // TODO: split into seperate stream class so we can have multiple file streams in parallel.
        NabtoDeviceConnectionRef ref = nabto_device_stream_get_connection_ref(stream);
        if (self->fileStream_ == NULL && nm_iam_check_access(&self->iam_,ref, "Webrtc:FileStream", NULL))
        {
            // if we don't already have an open file stream
            // and IAM allowed the stream
            self->fileStream_ = stream;
            self->me_ = self; // keep self alive until stream is closed
            nabto_device_stream_accept(self->fileStream_, self->fileStreamFut_);
            nabto_device_future_set_callback(self->fileStreamFut_, fileStreamAccepted, self.get());
        }
        else {
            NPLOGI << "FileStream opened, but " << (self->fileStream_ == NULL ? "IAM rejected it" : "another is already opened");
            nabto_device_stream_free(stream);
        }

    });
    return true;
}



void NabtoDeviceApp::fileStreamAccepted(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    NabtoDeviceApp* self = (NabtoDeviceApp*)userData;
    if (ec != NABTO_DEVICE_EC_OK) {
        NPLOGE << "file stream accept failed";
        self->evQueue_->post([self]() {
            self->fileStream_ = NULL;
            self->me_ = nullptr;
        });
        return;
    }
    self->evQueue_->post([self]() {
        self->inputFile_ = std::ifstream("nabto.png", std::ifstream::binary);
        self->doStreamFile();
    });
}

void NabtoDeviceApp::doStreamFile()
{
    if (!inputFile_.eof()) {
        inputFile_.read(fileBuffer_.data(), fileBuffer_.size());
        std::streamsize s = inputFile_.gcount();
        NPLOGD << "Read " << s << "bytes, writing to nabto stream";
        nabto_device_stream_write(fileStream_, fileStreamFut_, fileBuffer_.data(), s);
        nabto_device_future_set_callback(fileStreamFut_, writeFileStreamCb, this);
    } else {
        NPLOGD << "File reached EOF, closing nabto stream";
        nabto_device_stream_close(fileStream_, fileStreamFut_);
        nabto_device_future_set_callback(fileStreamFut_, closeFileStreamCb, this);
    }

}

void NabtoDeviceApp::writeFileStreamCb(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    NabtoDeviceApp* self = (NabtoDeviceApp*)userData;
    if (ec != NABTO_DEVICE_EC_OK) {
        NPLOGD << "file stream write failed";
        self->evQueue_->post([self]() {
            self->fileStream_ = NULL;
            self->me_ = nullptr;
        });
        return;
    }
    self->evQueue_->post([self]() {
        self->doStreamFile();
    });

}

void NabtoDeviceApp::closeFileStreamCb(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
{
    NabtoDeviceApp* self = (NabtoDeviceApp*)userData;
    if (ec != NABTO_DEVICE_EC_OK) {
        NPLOGI << "file stream close failed";
        self->evQueue_->post([self]() {
            self->fileStream_ = NULL;
            self->me_ = nullptr;
        });
        return;
    }
    NPLOGD << "Stream closed";
    nabto_device_stream_abort(self->fileStream_);
    // We never read, we are not writing, we are done closeing, so we do not have unresolved futures.
    nabto_device_stream_free(self->fileStream_);
}

uint32_t NabtoDeviceApp::getFileStreamPort()
{
    return fileStreamListener_->getStreamPort();
}


void NabtoDeviceApp::stop() {
    return nabto_device_stop(device_.get());
}

void NabtoDeviceApp::handleOauthRequest(NabtoDeviceCoapRequest* coap) {
    uint16_t cf; // expect content format text/plain: cf == 0
    NabtoDeviceError ec;
    if ((ec = nabto_device_coap_request_get_content_format(coap, &cf)) != NABTO_DEVICE_EC_OK || cf != 0) {
        NPLOGE << "  Invalid content format: " << cf << " ec: " << nabto_device_error_get_message(ec);
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

    NabtoOauthValidatorPtr oauth = std::make_shared<NabtoOauthValidator>(jwksUrl_, jwksIssuer_, productId_, deviceId_, caBundle_);

    if (!oauth->validateToken(token, [self, coap](bool valid, std::string subject) {
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
    })) {
        nabto_device_coap_error_response(coap, 500, "Internal Error");
        nabto_device_coap_request_free(coap);
    }

}


void NabtoDeviceApp::handleChallengeRequest(NabtoDeviceCoapRequest* coap) {
    uint16_t cf; // expect content format application/json: cf == 50
    NabtoDeviceError ec;
    if ((ec = nabto_device_coap_request_get_content_format(coap, &cf)) != NABTO_DEVICE_EC_OK || cf != NABTO_DEVICE_COAP_CONTENT_FORMAT_APPLICATION_JSON) {
        NPLOGE << "  Invalid content format: " << cf << " ec: " << nabto_device_error_get_message(ec);
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
        NPLOGE << "Failed to parse json payload: " << payloadStr;
        nabto_device_coap_error_response(coap, 400, "Invalid JSON");
        nabto_device_coap_request_free(coap);
        return;
    }

    char* deviceFp = NULL;
    char* clientFp = NULL;

    NabtoDeviceConnectionRef ref = nabto_device_coap_request_get_connection_ref(coap);

    if (nabto_device_connection_get_client_fingerprint(device_.get(), ref, &clientFp) != NABTO_DEVICE_EC_OK) {
        NPLOGE << "Failed to get client fingerprint";
        nabto_device_coap_error_response(coap, 400, "Invalid Connection");
        nabto_device_coap_request_free(coap);
        return;
    }

    if (nabto_device_connection_get_device_fingerprint(device_.get(), ref, &deviceFp) != NABTO_DEVICE_EC_OK) {
        nabto_device_string_free(clientFp);
        NPLOGE << "Failed to get device fingerprint";
        nabto_device_coap_error_response(coap, 400, "Invalid Connection");
        nabto_device_coap_request_free(coap);
        return;
    }

    // TODO: dont require jwksurl, issuer, product id, device id for this
    NabtoOauthValidatorPtr oauth = std::make_shared<NabtoOauthValidator>(jwksUrl_, jwksIssuer_, productId_, deviceId_, caBundle_);

    std::string devFp(deviceFp, 64);
    std::string cliFp(clientFp, 64);

    auto token = oauth->createChallengeResponse(rawPrivateKey_, cliFp, devFp, nonce);

    nlohmann::json resp = {{"response", token}};
    NPLOGD << "Sending info response: " << resp.dump();
    auto respPayload = resp.dump(); //nlohmann::json::to_cbor(resp);
    nabto_device_coap_response_set_code(coap, 205);
    nabto_device_coap_response_set_content_format(coap, NABTO_DEVICE_COAP_CONTENT_FORMAT_APPLICATION_JSON);
    nabto_device_coap_response_set_payload(coap, respPayload.data(), respPayload.size());
    nabto_device_coap_response_ready(coap);
    nabto_device_coap_request_free(coap);
    nabto_device_string_free(clientFp);
    nabto_device_string_free(deviceFp);
}

void NabtoDeviceApp::iamLogger(void* data, enum nn_log_severity severity, const char* module,
    const char* file, int line,
    const char* fmt, va_list args)
{
    NabtoDeviceApp* self = (NabtoDeviceApp*)data;
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

void NabtoDeviceApp::createHomeDir(const std::string& homedir) {
#if HAVE_FILESYSTEM_H
    std::filesystem::create_directories(homedir);
#else
    NPLOGD << "Cannot create homedir " << homedir << " since the toolchain does not provide std::filesystem";
#endif
}


NabtoDeviceStreamListenerPtr NabtoDeviceStreamListener::create(NabtoDeviceAppPtr device, nabto::EventQueuePtr queue)
{
    auto ptr = std::make_shared<NabtoDeviceStreamListener>(device, queue);
    if (ptr->start()) {
        return ptr;
    }
    return nullptr;
}

NabtoDeviceStreamListener::NabtoDeviceStreamListener(NabtoDeviceAppPtr device, nabto::EventQueuePtr queue) : device_(device), queue_(queue)
{
    streamListen_ = nabto_device_listener_new(device_->getDevice().get());
    streamFut_ = nabto_device_future_new(device_->getDevice().get());
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
        nabto_device_stream_init_listener_ephemeral(device_->getDevice().get(), streamListen_, &streamPort_) != NABTO_DEVICE_EC_OK)
    {
        NPLOGE << "Failed to listen for streams";
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
        NPLOGD << "stream future wait failed: " << nabto_device_error_get_message(ec);
        self->queue_->post([self]() {
            self->me_ = nullptr;
            self->streamCb_ = nullptr;
            self->device_ = nullptr;
            });
        return;
    }
    std::function<void(NabtoDeviceStream* coap)> cb = self->streamCb_;
    NabtoDeviceStream* stream = self->stream_;
    self->queue_->post([cb, stream]() {
        cb(stream);
        });
    self->stream_ = NULL;
    self->nextStream();

}


NabtoDeviceCoapListenerPtr NabtoDeviceCoapListener::create(NabtoDeviceAppPtr device, NabtoDeviceCoapMethod method, const char** path, nabto::EventQueuePtr queue)
{
    auto ptr = std::make_shared<NabtoDeviceCoapListener>(device, queue);
    if (ptr->start(method, path)) {
        return ptr;
    }
    return nullptr;

}

NabtoDeviceCoapListener::NabtoDeviceCoapListener(NabtoDeviceAppPtr device, nabto::EventQueuePtr queue) : device_(device), queue_(queue)
{
    listener_ = nabto_device_listener_new(device_->getDevice().get());
    future_ = nabto_device_future_new(device_->getDevice().get());

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
        nabto_device_coap_init_listener(device_->getDevice().get(), listener_, method, path) != NABTO_DEVICE_EC_OK)
    {
        NPLOGE << "Failed to listen for CoAP requests";
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
        NPLOGD << "Coap listener future wait failed: " << nabto_device_error_get_message(ec);
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
