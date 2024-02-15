#pragma once

#include "webrtc_coap_channel.hpp"
#include "webrtc_stream_channel.hpp"

#include <signaling-stream/signaling_stream_ptr.hpp>

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

    static WebrtcConnectionPtr create(SignalingStreamPtr sigStream, NabtoDevicePtr device, std::vector<struct TurnServer>& turnServers, EventQueuePtr queue, TrackEventCallback trackCb, CheckAccessCallback accessCb);
    WebrtcConnection(SignalingStreamPtr sigStream, NabtoDevicePtr device, std::vector<struct TurnServer>& turnServers, EventQueuePtr queue, TrackEventCallback trackCb, CheckAccessCallback accessCb);
    ~WebrtcConnection();


    void handleOfferAnswer(const std::string &data, const nlohmann::json& metadata );

    void handleIce(std::string& data);

    void createTracks(std::vector<MediaTrackPtr>& tracks);

    void setMetadata(const nlohmann::json& metadata)
    {
        metadata_ = metadata;
        try {
            if (metadata.contains("noTrickle")) {
                bool noTrickle = metadata["noTrickle"].get<bool>();
                if (noTrickle) {
                    canTrickle_ = false;
                }
            }
        } catch(std::exception& ex) {
            // Ignore missing noTrickle
        }
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
    void acceptTrack(MediaTrackPtr track);
    MediaTrackPtr createMediaTrack(std::shared_ptr<rtc::Track> track);

    NabtoDeviceConnectionRef getConnectionRef();
    void updateMetaTracks();
    std::string trackErrorToString(enum MediaTrack::ErrorState state);

    void sendDescription(rtc::optional<rtc::Description> description);
    void maybeNegotiationNeeded();
    void onNegotiationNeeded();
    void handleSignalingMessage(rtc::optional<rtc::Description> description, const nlohmann::json& metadata);

    SignalingStreamPtr sigStream_;
    NabtoDevicePtr device_;
    std::vector<struct TurnServer> turnServers_;
    EventQueuePtr queue_;
    TrackEventCallback trackCb_;
    CheckAccessCallback accessCb_;
    EventQueueWork queueWork_;
    enum ConnectionState state_ = CREATED;

    nlohmann::json metadata_;
    std::function<void(enum ConnectionState)> eventHandler_;

    std::shared_ptr<rtc::PeerConnection> pc_ = nullptr;
    NabtoDeviceVirtualConnection* nabtoConnection_ = NULL;
    WebrtcCoapChannelPtr coapChannel_ = nullptr;
    WebrtcStreamChannelPtr streamChannel_ = nullptr;

    bool canTrickle_ = true;
    std::vector<MediaTrackPtr> mediaTracks_;

    // State for Perfect Negotiation
    bool polite_ = false;
    bool makingOffer_ = false;
    bool ignoreOffer_ = false;
};


} // namespace
