#pragma once

#include <nabto/nabto_device.h>
#include <nabto/nabto_device_webrtc.hpp>

#include <nlohmann/json.hpp>

#include <iostream>
#include <memory>

#include <fstream>

class CoapListener;

typedef std::shared_ptr<CoapListener> CoapListenerPtr;

class CoapListener : public std::enable_shared_from_this <CoapListener> {
public:
    static CoapListenerPtr create(nabto::NabtoDevicePtr device, NabtoDeviceCoapMethod method, std::vector<std::string> path, nabto::EventQueuePtr queue);
    CoapListener(nabto::NabtoDevicePtr device, nabto::EventQueuePtr queue);
    ~CoapListener();

    bool start(NabtoDeviceCoapMethod method, std::vector<std::string> path);

    void setCoapCallback(std::function<void(NabtoDeviceCoapRequest* coap)> coapCb) { coapCb_ = coapCb; }

private:
    void nextCoapRequest();
    static void newCoapRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);

    nabto::NabtoDevicePtr device_;
    nabto::EventQueuePtr queue_;
    std::function<void(NabtoDeviceCoapRequest* coap)> coapCb_;

    NabtoDeviceListener* listener_ = NULL;
    NabtoDeviceFuture* future_ = NULL;
    NabtoDeviceCoapRequest* coap_ = NULL;

    CoapListenerPtr me_ = nullptr;
};
