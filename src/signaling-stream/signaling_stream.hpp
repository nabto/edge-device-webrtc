#pragma once

#include "signaling_stream_ptr.hpp"
#include <webrtc-connection/webrtc_connection.hpp>
#include <media-streams/media_stream.hpp>

#include <nabto-device/nabto_device.hpp>
#include <nabto/nabto_device_experimental.h>
#include <nabto/nabto_device_virtual.h>

#include <memory>

namespace nabto {

class SignalingStream : public std::enable_shared_from_this<SignalingStream>
{
public:
    enum ObjectType {
        WEBRTC_OFFER = 0,
        WEBRTC_ANSWER,
        WEBRTC_ICE,
        TURN_REQUEST,
        TURN_RESPONSE
    };

    static SignalingStreamPtr create(NabtoDeviceImplPtr device, NabtoDeviceStream* stream, SignalingStreamManagerPtr manager, std::vector<nabto::MediaStreamPtr>& medias);

    SignalingStream(NabtoDeviceImplPtr device, NabtoDeviceStream* stream, SignalingStreamManagerPtr manager, std::vector<nabto::MediaStreamPtr>& medias);

    ~SignalingStream();

    void start();

    // Used by the WebrtcConnection to send signaling messages. These are fire-and-forget for the WebrtcConnection as these failing will mean the Nabto stream has failed. The Nabto stream failing will cause things to shutdown from another place.
    void signalingSendOffer(std::string& data, nlohmann::json& metadata);
    void signalingSendAnswer(std::string& data, nlohmann::json& metadata);
    void signalingSendIce(std::string& data, nlohmann::json& metadata);

private:
    static void iceServersResolved(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);
    void parseIceServers();
    void createWebrtcConnection();

    void sendSignalligObject(std::string& data);
    static void streamWriteCallback(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);

    static void streamAccepted(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);

    void readObjLength();
    static void hasReadObjLen(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);
    void handleReadObjLen();
    void readObject(uint32_t len);
    static void hasReadObject(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);
    void handleReadObject();

    void sendTurnServers();

    void closeStream();
    static void streamClosed(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);

    void cleanup();


    NabtoDeviceImplPtr device_;
    NabtoDeviceStream* stream_;
    SignalingStreamManagerPtr manager_;
    std::vector<nabto::MediaStreamPtr> medias_;
    NabtoDeviceFuture* future_;
    NabtoDeviceFuture* writeFuture_;

    size_t readLength_;
    uint32_t objectLength_;
    uint8_t* objectBuffer_;
    bool accepted_ = false;
    bool closed_ = false;
    bool reading_ = false;

    uint8_t* writeBuf_ = NULL;

    NabtoDeviceIceServersRequest* iceReq_;
    std::vector<WebrtcConnection::TurnServer> turnServers_;
    WebrtcConnectionPtr webrtcConnection_;
    SignalingStreamPtr self_;



};


} // namespace
