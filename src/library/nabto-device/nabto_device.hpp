#pragma once

#include <nabto/nabto_device.h>
#include <modules/iam/nm_iam.h>
#include <nabto/nabto_device_webrtc.hpp>

#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>

#include <fstream>

namespace nabto {

class NabtoDeviceStreamListener;
class NabtoDeviceCoapListener;

typedef std::shared_ptr<NabtoDeviceStreamListener> NabtoDeviceStreamListenerPtr;
typedef std::shared_ptr<NabtoDeviceCoapListener> NabtoDeviceCoapListenerPtr;

class NabtoDeviceStreamListener : public std::enable_shared_from_this <NabtoDeviceStreamListener> {
public:
    static NabtoDeviceStreamListenerPtr create(NabtoDevicePtr device, EventQueuePtr queue);
    NabtoDeviceStreamListener(NabtoDevicePtr device, EventQueuePtr queue);
    ~NabtoDeviceStreamListener();

    bool start();

    void setStreamCallback(std::function<void(NabtoDeviceStream* stream)> streamCb) { streamCb_ = streamCb; }

    uint32_t getStreamPort() { return streamPort_; }
private:
    void nextStream();
    static void newStream(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);


    NabtoDevicePtr device_;
    EventQueuePtr queue_;
    uint32_t streamPort_;
    std::function<void(NabtoDeviceStream* stream)> streamCb_;

    NabtoDeviceListener* streamListen_ = NULL;
    NabtoDeviceFuture* streamFut_ = NULL;
    NabtoDeviceStream* stream_ = NULL;

    NabtoDeviceStreamListenerPtr me_ = nullptr;

};

class NabtoDeviceCoapListener : public std::enable_shared_from_this <NabtoDeviceCoapListener> {
public:
    static NabtoDeviceCoapListenerPtr create(NabtoDevicePtr device, NabtoDeviceCoapMethod method, const char** path, EventQueuePtr queue);
    NabtoDeviceCoapListener(NabtoDevicePtr device, EventQueuePtr queue);
    ~NabtoDeviceCoapListener();

    bool start(NabtoDeviceCoapMethod method, const char** path);

    void setCoapCallback(std::function<void(NabtoDeviceCoapRequest* coap)> coapCb) { coapCb_ = coapCb; }

private:
    void nextCoapRequest();
    static void newCoapRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);

    NabtoDevicePtr device_;
    EventQueuePtr queue_;
    std::function<void(NabtoDeviceCoapRequest* coap)> coapCb_;

    NabtoDeviceListener* listener_ = NULL;
    NabtoDeviceFuture* future_ = NULL;
    NabtoDeviceCoapRequest* coap_ = NULL;

    NabtoDeviceCoapListenerPtr me_ = nullptr;

};

} // namespace
