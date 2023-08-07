#pragma once

#include "webrtc_coap_channel.hpp"

#include <signaling-stream/signaling_stream_ptr.hpp>
#include <nabto-device/nabto_device.hpp>
#include <media-streams/media_stream.hpp>

#include <nabto/nabto_device_virtual.h>

#include <rtc/rtc.hpp>
#include <nlohmann/json.hpp>

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

    static WebrtcConnectionPtr create(SignalingStreamPtr sigStream, NabtoDeviceImplPtr device, std::vector<struct TurnServer>& turnServers, std::vector<nabto::MediaStreamPtr>& medias);
    WebrtcConnection(SignalingStreamPtr sigStream, NabtoDeviceImplPtr device, std::vector<struct TurnServer>& turnServers, std::vector<nabto::MediaStreamPtr>& medias);
    ~WebrtcConnection();

    void handleOffer(std::string& data);

    void handleAnswer(std::string& data);

    void handleIce(std::string& data);

    void handleOfferRequest();

    void createTrack(nabto::MediaStreamPtr media)
    {
        media->createTrack(pc_);
        pc_->setLocalDescription();
    }

    void setMetadata(nlohmann::json& metadata)
    {
        metadata_ = metadata;
    }

    void setEventHandler(std::function<void(enum ConnectionState)> eventHandler)
    {
        eventHandler_ = eventHandler;
    }

    void stop();

    bool isConnection(NabtoDeviceConnectionRef ref)
    {
        if (nabtoConnection_ != NULL) {
            NabtoDeviceConnectionRef me = nabto_device_connection_get_connection_ref(nabtoConnection_);
            return ref == me;
        }
        return false;
    }

private:

    void createPeerConnection();
    void handleSignalingStateChange(rtc::PeerConnection::SignalingState state);
    void handleTrackEvent(std::shared_ptr<rtc::Track> track);
    void handleDatachannelEvent(std::shared_ptr<rtc::DataChannel> incoming);

    SignalingStreamPtr sigStream_;
    NabtoDeviceImplPtr device_;
    std::vector<struct TurnServer> turnServers_;
    std::vector<nabto::MediaStreamPtr> medias_;
    enum ConnectionState state_ = CREATED;

    nlohmann::json metadata_;
    std::function<void(enum ConnectionState)> eventHandler_;

    std::shared_ptr<rtc::PeerConnection> pc_ = nullptr;
    NabtoDeviceVirtualConnection* nabtoConnection_ = NULL;
    WebrtcCoapChannelPtr coapChannel_ = nullptr;


};


} // namespace
