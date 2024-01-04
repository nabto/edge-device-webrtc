#pragma once

#include <nabto/nabto_device.h>
#include <modules/iam/nm_iam.h>
#include <nabto/nabto_device_webrtc.hpp>

#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>

#include <fstream>

namespace nabto {

class NabtoStreamListener;
class NabtoCoapListener;

typedef std::shared_ptr<NabtoStreamListener> NabtoStreamListenerPtr;
typedef std::shared_ptr<NabtoCoapListener> NabtoCoapListenerPtr;

class NabtoStreamListener : public std::enable_shared_from_this <NabtoStreamListener> {
public:
    static NabtoStreamListenerPtr create(NabtoDevicePtr device, EventQueuePtr queue);
    NabtoStreamListener(NabtoDevicePtr device, EventQueuePtr queue);
    ~NabtoStreamListener();

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

    NabtoStreamListenerPtr me_ = nullptr;

};

class NabtoCoapListener : public std::enable_shared_from_this <NabtoCoapListener> {
public:
    static NabtoCoapListenerPtr create(NabtoDevicePtr device, NabtoDeviceCoapMethod method, const char** path, EventQueuePtr queue);
    NabtoCoapListener(NabtoDevicePtr device, EventQueuePtr queue);
    ~NabtoCoapListener();

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

    NabtoCoapListenerPtr me_ = nullptr;

};

} // namespace
