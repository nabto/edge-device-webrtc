#pragma once

#include <nabto-device/nabto_device.hpp>
#include <signaling-stream/signaling_stream.hpp>
#include <media-streams/media_stream.hpp>

#include <memory>

namespace nabto {

class SignalingStreamManager;

typedef std::shared_ptr<SignalingStreamManager> SignalingStreamManagerPtr;

class SignalingStreamManager : public std::enable_shared_from_this<SignalingStreamManager>
{
public:
    static SignalingStreamManagerPtr create(NabtoDeviceImplPtr device, std::vector<nabto::MediaStreamPtr>& medias);
    SignalingStreamManager(NabtoDeviceImplPtr device, std::vector<nabto::MediaStreamPtr>& medias);
    ~SignalingStreamManager();

    bool start();

private:
    void nextInfoRequest();
    static void newInfoRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);

    void nextVideoRequest();
    static void newVideoRequest(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);
    void handleVideoRequest();

    NabtoDeviceImplPtr device_;
    std::vector<nabto::MediaStreamPtr> medias_;

    NabtoDeviceListener* coapInfoListener_;
    NabtoDeviceFuture* coapInfoListenFuture_;
    NabtoDeviceCoapRequest* coapInfoRequest_;

    NabtoDeviceListener* coapVideoListener_;
    NabtoDeviceFuture* coapVideoListenFuture_;
    NabtoDeviceCoapRequest* coapVideoRequest_;

    NabtoDeviceStreamListenerPtr streamListener_;

    std::vector<SignalingStreamWeakPtr> streams_;
    SignalingStreamManagerPtr me_ = nullptr;

};


} // namespace
