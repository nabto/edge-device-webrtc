#pragma once

#include <nabto/nabto_device.h>
#include <modules/iam/nm_iam.h>

#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>

#include <fstream>

namespace nabto {

class NabtoDeviceImpl;
class NabtoDeviceStreamListener;
class NabtoDeviceCoapListener;

typedef std::shared_ptr<NabtoDeviceImpl> NabtoDeviceImplPtr;
typedef std::shared_ptr<NabtoDeviceStreamListener> NabtoDeviceStreamListenerPtr;
typedef std::shared_ptr<NabtoDeviceCoapListener> NabtoDeviceCoapListenerPtr;

class NabtoDeviceImpl : public std::enable_shared_from_this <NabtoDeviceImpl> {
public:
    static NabtoDeviceImplPtr create(nlohmann::json& opts);
    NabtoDeviceImpl();
    ~NabtoDeviceImpl();
    bool init(nlohmann::json& opts);

    void setIamConfigFile(std::string& path) { iamConfPath_ = path; }
    void setIamStateFile(std::string& path) { iamStatePath_ = path; }

    bool start();
    void stop();

    NabtoDevice* getDevice() { return device_; }
    uint32_t getFileStreamPort();
    struct nm_iam* getIam() { return &iam_; }

private:

    bool setupIam();
    bool createDefaultIamState();
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

    std::vector<char> fileBuffer_;
    NabtoDevice* device_;
    std::string productId_;
    std::string deviceId_;
    std::string rawPrivateKey_;
    std::string sct_ = "demosct";
    std::string logLevel_ = "info";
    enum nn_log_severity iamLogLevel_ = NN_LOG_SEVERITY_INFO;
    std::string serverUrl_;

    NabtoDeviceStreamListenerPtr fileStreamListener_ = nullptr;
    std::ifstream inputFile_;
    NabtoDeviceFuture* fileStreamFut_ = NULL;
    NabtoDeviceStream* fileStream_ = NULL;

    NabtoDeviceImplPtr me_ = nullptr;

    struct nm_iam iam_;
    struct nn_log iamLog_;
    std::string iamConfPath_ = "iam_config.json";
    std::string iamStatePath_ = "iam_state.json";

};


class NabtoDeviceStreamListener : public std::enable_shared_from_this <NabtoDeviceStreamListener> {
public:
    static NabtoDeviceStreamListenerPtr create(NabtoDeviceImplPtr device);
    NabtoDeviceStreamListener(NabtoDeviceImplPtr device);
    ~NabtoDeviceStreamListener();

    bool start();

    void setStreamCallback(std::function<void(NabtoDeviceStream* stream)> streamCb) { streamCb_ = streamCb; }

    uint32_t getStreamPort() { return streamPort_; }
private:
    void nextStream();
    static void newStream(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);


    NabtoDeviceImplPtr device_;
    uint32_t streamPort_;
    std::function<void(NabtoDeviceStream* stream)> streamCb_;

    NabtoDeviceListener* streamListen_ = NULL;
    NabtoDeviceFuture* streamFut_ = NULL;
    NabtoDeviceStream* stream_ = NULL;

    NabtoDeviceStreamListenerPtr me_ = nullptr;

};

class NabtoDeviceCoapListener : public std::enable_shared_from_this <NabtoDeviceCoapListener> {
public:
    static NabtoDeviceCoapListenerPtr create(NabtoDeviceImplPtr device, NabtoDeviceCoapMethod method, const char** path);
    NabtoDeviceCoapListener(NabtoDeviceImplPtr device);
    ~NabtoDeviceCoapListener();

    bool start(NabtoDeviceCoapMethod method, const char** path);

    void setCoapCallback(std::function<void(NabtoDeviceCoapRequest* coap)> coapCb) { coapCb_ = coapCb; }

private:
    void nextCoapRequest();
    static void newCoapRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);


    NabtoDeviceImplPtr device_;
    std::function<void(NabtoDeviceCoapRequest* coap)> coapCb_;

    NabtoDeviceListener* listener_ = NULL;
    NabtoDeviceFuture* future_ = NULL;
    NabtoDeviceCoapRequest* coap_ = NULL;

    NabtoDeviceCoapListenerPtr me_ = nullptr;

};

} // namespace
