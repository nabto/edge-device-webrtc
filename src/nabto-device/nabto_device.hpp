#pragma once

#include <nabto/nabto_device.h>

#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>

#include <fstream>

namespace nabto {

class NabtoDeviceImpl;

typedef std::shared_ptr<NabtoDeviceImpl> NabtoDeviceImplPtr;

class NabtoDeviceImpl : public std::enable_shared_from_this <NabtoDeviceImpl> {
public:
    static NabtoDeviceImplPtr create(nlohmann::json& opts);
    NabtoDeviceImpl();
    ~NabtoDeviceImpl();
    bool init(nlohmann::json& opts);

    bool start();
    void stop();

    NabtoDevice* getDevice() { return device_; }

private:

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

    NabtoDeviceListener* fileStreamListen_ = NULL;
    NabtoDeviceFuture* fileStreamFut_ = NULL;
    NabtoDeviceStream* fileStream_ = NULL;
    std::ifstream inputFile_;
};

} // namespace
