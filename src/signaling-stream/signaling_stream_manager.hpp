#pragma once

#include <nabto-device/nabto_device.hpp>
#include <signaling-stream/signaling_stream.hpp>

#include <memory>

namespace nabto {

class SignalingStreamManager;

typedef std::shared_ptr<SignalingStreamManager> SignalingStreamManagerPtr;

class SignalingStreamManager : public std::enable_shared_from_this<SignalingStreamManager>
{
public:
    static SignalingStreamManagerPtr create(NabtoDeviceImplPtr device);
    SignalingStreamManager(NabtoDeviceImplPtr device);
    ~SignalingStreamManager();

    bool start();

private:
    void nextStream();
    static void newStream(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);

    void nextCoapRequest();
    static void newCoapRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);

    NabtoDeviceImplPtr device_;

    NabtoDeviceListener* coapListener_;
    NabtoDeviceFuture* coapListenFuture_;
    NabtoDeviceCoapRequest* coapRequest_;

    uint32_t streamPort_ = 0;
    NabtoDeviceListener* streamListener_;
    NabtoDeviceFuture* streamListenFuture_;
    NabtoDeviceStream* stream_;

    std::vector<SignalingStreamWeakPtr> streams_;
    SignalingStreamManagerPtr me_ = nullptr;

};


} // namespace
