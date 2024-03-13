#pragma once

#include <nabto/nabto_device.h>
#include <modules/iam/nm_iam.h>
#include <nabto/nabto_device_webrtc.hpp>

#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>

#include <fstream>

namespace example {

class NabtoDeviceApp;
class NabtoDeviceStreamListener;
class NabtoDeviceCoapListener;

typedef std::shared_ptr<NabtoDeviceApp> NabtoDeviceAppPtr;
typedef std::shared_ptr<NabtoDeviceStreamListener> NabtoDeviceStreamListenerPtr;
typedef std::shared_ptr<NabtoDeviceCoapListener> NabtoDeviceCoapListenerPtr;

class NabtoDeviceApp : public std::enable_shared_from_this <NabtoDeviceApp> {
public:
    static NabtoDeviceAppPtr create(nlohmann::json& opts, nabto::EventQueuePtr queue);
    NabtoDeviceApp(nabto::EventQueuePtr queue);
    ~NabtoDeviceApp();
    bool init(nlohmann::json& opts);

    void setIamConfigFile(std::string& path) { iamConfPath_ = path; }
    void setIamStateFile(std::string& path) { iamStatePath_ = path; }
    void setJwksConfig(std::string& url, std::string& issuer) { jwksUrl_ = url; jwksIssuer_ = issuer; }

    bool start();
    void stop();

    nabto::NabtoDevicePtr getDevice() { return device_; }
    uint32_t getFileStreamPort();
    struct nm_iam* getIam() { return &iam_; }

    bool resetIam();

private:

    bool setupIam(const char* fp);
    bool createDefaultIamState();
    bool createDefaultIamConfig();
    static void iamLogger(void* data, enum nn_log_severity severity, const char* module,
        const char* file, int line,
        const char* fmt, va_list args);
    static void iamStateChanged(struct nm_iam* iam, void* userdata);

    bool setupFileStream();
    void nextFileStream();
    static void newFileStream(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);
    static void fileStreamAccepted(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);
    void doStreamFile();
    static void writeFileStreamCb(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);
    static void closeFileStreamCb(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);

    void handleOauthRequest(NabtoDeviceCoapRequest* req);
    void handleChallengeRequest(NabtoDeviceCoapRequest* req);

    nabto::EventQueuePtr evQueue_;
    std::vector<char> fileBuffer_;
    nabto::NabtoDevicePtr device_;
    std::string productId_;
    std::string deviceId_;
    std::string rawPrivateKey_;
    std::string logLevel_ = "info";
    std::optional<std::string> caBundle_;

    enum nn_log_severity iamLogLevel_ = NN_LOG_SEVERITY_INFO;
    std::string serverUrl_;

    NabtoDeviceStreamListenerPtr fileStreamListener_ = nullptr;
    std::ifstream inputFile_;
    NabtoDeviceFuture* fileStreamFut_ = NULL;
    NabtoDeviceStream* fileStream_ = NULL;

    NabtoDeviceCoapListenerPtr coapOauthListener_ = nullptr;
    NabtoDeviceCoapListenerPtr coapChallengeListener_ = nullptr;

    NabtoDeviceAppPtr me_ = nullptr;

    struct nm_iam iam_;
    struct nn_log iamLog_;
    std::string iamConfPath_ = "iam_config.json";
    std::string iamStatePath_ = "iam_state.json";

    std::string jwksUrl_ = "http://localhost:3000/jwks";
    std::string jwksIssuer_ = "http://localhost:3000";

    std::string frontendUrl_ = "https://smartcloud.tk.dev.nabto.com/";
    bool iamReset_ = false;

};


class NabtoDeviceStreamListener : public std::enable_shared_from_this <NabtoDeviceStreamListener> {
public:
    static NabtoDeviceStreamListenerPtr create(NabtoDeviceAppPtr device, nabto::EventQueuePtr queue);
    NabtoDeviceStreamListener(NabtoDeviceAppPtr device, nabto::EventQueuePtr queue);
    ~NabtoDeviceStreamListener();

    bool start();

    void setStreamCallback(std::function<void(NabtoDeviceStream* stream)> streamCb) { streamCb_ = streamCb; }

    uint32_t getStreamPort() { return streamPort_; }
private:
    void nextStream();
    static void newStream(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);


    NabtoDeviceAppPtr device_;
    nabto::EventQueuePtr queue_;
    uint32_t streamPort_;
    std::function<void(NabtoDeviceStream* stream)> streamCb_;

    NabtoDeviceListener* streamListen_ = NULL;
    NabtoDeviceFuture* streamFut_ = NULL;
    NabtoDeviceStream* stream_ = NULL;

    NabtoDeviceStreamListenerPtr me_ = nullptr;

};

class NabtoDeviceCoapListener : public std::enable_shared_from_this <NabtoDeviceCoapListener> {
public:
    static NabtoDeviceCoapListenerPtr create(NabtoDeviceAppPtr device, NabtoDeviceCoapMethod method, const char** path, nabto::EventQueuePtr queue);
    NabtoDeviceCoapListener(NabtoDeviceAppPtr device, nabto::EventQueuePtr queue);
    ~NabtoDeviceCoapListener();

    bool start(NabtoDeviceCoapMethod method, const char** path);

    void setCoapCallback(std::function<void(NabtoDeviceCoapRequest* coap)> coapCb) { coapCb_ = coapCb; }

private:
    void nextCoapRequest();
    static void newCoapRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);

    NabtoDeviceAppPtr device_;
    nabto::EventQueuePtr queue_;
    std::function<void(NabtoDeviceCoapRequest* coap)> coapCb_;

    NabtoDeviceListener* listener_ = NULL;
    NabtoDeviceFuture* future_ = NULL;
    NabtoDeviceCoapRequest* coap_ = NULL;

    NabtoDeviceCoapListenerPtr me_ = nullptr;

};

} // namespace
