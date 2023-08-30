#pragma once

#include <nabto/nabto_device.h>

#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>

#include <fstream>

namespace nabto {

class NabtoDeviceImpl;
class NabtoDeviceStreamListener;

typedef std::shared_ptr<NabtoDeviceImpl> NabtoDeviceImplPtr;
typedef std::shared_ptr<NabtoDeviceStreamListener> NabtoDeviceStreamListenerPtr;

class NabtoDeviceImpl : public std::enable_shared_from_this <NabtoDeviceImpl> {
public:
    static NabtoDeviceImplPtr create(nlohmann::json& opts);
    NabtoDeviceImpl();
    ~NabtoDeviceImpl();
    bool init(nlohmann::json& opts);

    bool start();
    void stop();

    NabtoDevice* getDevice() { return device_; }
    uint32_t getFileStreamPort();

private:

    bool setupPassword();
    void nextPasswordRequest();
    static void newPasswordRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);

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
    std::string serverUrl_;

    NabtoDeviceStreamListenerPtr fileStreamListener_ = nullptr;
    std::ifstream inputFile_;
    NabtoDeviceFuture* fileStreamFut_ = NULL;
    NabtoDeviceStream* fileStream_ = NULL;

    NabtoDeviceListener* passwordListen_ = NULL;
    NabtoDeviceFuture* passwordFut_ = NULL;
    NabtoDevicePasswordAuthenticationRequest* passwordReq_ = NULL;

    NabtoDeviceImplPtr me_ = nullptr;

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

} // namespace
