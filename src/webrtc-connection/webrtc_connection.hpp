#pragma once

#include "webrtc_coap_channel.hpp"

#include <signaling-stream/signaling_stream_ptr.hpp>
#include <nabto-device/nabto_device.hpp>

#include <nabto/nabto_device_virtual.h>

#include <rtc/rtc.hpp>

#include <memory>
#include <vector>

namespace nabto {

class WebrtcConnection;

typedef std::shared_ptr<WebrtcConnection> WebrtcConnectionPtr;

class WebrtcConnection : public std::enable_shared_from_this<WebrtcConnection>
{
public:
    struct TurnServer {
        std::string hostname;
        uint16_t port;
        std::string username;
        std::string password;
    };

    enum ConnectionState {
        CREATED = 0,
        CONNECTING,
        CONNECTED,
        CLOSED,
        FAILED
    };

    enum coapMessageType {
        COAP_REQUEST = 0,
        COAP_RESPONSE
    };

    static WebrtcConnectionPtr create(SignalingStreamPtr sigStream, NabtoDeviceImplPtr device, std::vector<struct TurnServer>& turnServers);
    WebrtcConnection(SignalingStreamPtr sigStream, NabtoDeviceImplPtr device, std::vector<struct TurnServer>& turnServers);
    ~WebrtcConnection();

    void handleOffer(std::string& data);

    void handleAnswer(std::string& data);

    void handleIce(std::string& data);

    void stop();

private:

    void createPeerConnection();
    void handleSignalingStateChange(rtc::PeerConnection::SignalingState state);
    void handleTrackEvent(std::shared_ptr<rtc::Track> track);
    void handleDatachannelEvent(std::shared_ptr<rtc::DataChannel> incoming);

    SignalingStreamPtr sigStream_;
    NabtoDeviceImplPtr device_;
    std::vector<struct TurnServer> turnServers_;
    enum ConnectionState state_ = CREATED;

    std::shared_ptr<rtc::PeerConnection> pc_ = nullptr;
    NabtoDeviceVirtualConnection* nabtoConnection_ = NULL;
    WebrtcCoapChannelPtr coapChannel_ = nullptr;

};


} // namespace
