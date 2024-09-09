#pragma once

#include "signaling_stream_ptr.hpp"
#include <webrtc-connection/webrtc_connection.hpp>

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

    static SignalingStreamPtr create(NabtoDevicePtr device, NabtoDeviceStream* stream, SignalingStreamManagerPtr manager, EventQueuePtr queue, TrackEventCallback trackCb, CheckAccessCallback accessCb, DatachannelEventCallback datachannelCb, bool isV2);

    SignalingStream(NabtoDevicePtr device, NabtoDeviceStream* stream, SignalingStreamManagerPtr manager, EventQueuePtr queue, TrackEventCallback trackCb, CheckAccessCallback accessCb, DatachannelEventCallback datachannelCb, bool isV2);

    ~SignalingStream();

    void start();

    // Used by the WebrtcConnection to send signaling messages. These are fire-and-forget for the WebrtcConnection as these failing will mean the Nabto stream has failed. The Nabto stream failing will cause things to shutdown from another place.
    void signalingSendOffer(const std::string& data, const nlohmann::json& metadata);
    void signalingSendAnswer(const std::string& data, const nlohmann::json& metadata);
    void signalingSendIce(const std::string& data, const nlohmann::json& metadata);

    // Signaling v2
    void signalingSendDescription(const rtc::Description& desc, const nlohmann::json& metadata);
    void signalingSendCandidate(const rtc::Candidate& cand);

    bool isConnection(NabtoDeviceConnectionRef ref)
    {
        if (nabto_device_connection_is_virtual(device_.get(), ref)) {
            if (webrtcConnection_ != nullptr) {
                return webrtcConnection_->isConnection(ref);
            }
            return false;
        } else {
            NabtoDeviceConnectionRef me = nabto_device_stream_get_connection_ref(stream_);
            return me == ref;
        }
    }

    bool createTracks(const std::vector<MediaTrackPtr>& tracks)
    {
        if (webrtcConnection_ != nullptr) {
            try {
                webrtcConnection_->createTracks(tracks);
                return true;
            } catch (std::runtime_error& ex) {
                NPLOGE << "AcceptTrack runtime error: " << ex.what();
            }
        } else {
            deferredTracks_.insert(deferredTracks_.end(), tracks.begin(), tracks.end());
            return true;
        }
        return false;
    }

    NabtoDeviceConnectionRef getSignalingConnectionRef()
    {
        return nabto_device_stream_get_connection_ref(stream_);
    }

private:
    static void iceServersResolved(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);
    void parseIceServers();
    void createWebrtcConnection();

    void sendSignalligObject(const std::string& data);
    void tryWriteStream();
    static void streamWriteCallback(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);

    static void streamAccepted(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);

    void readObjLength();
    static void hasReadObjLen(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);
    void handleReadObjLen();
    void readObject(uint32_t len);
    static void hasReadObject(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);
    void handleReadObject();

    void sendTurnServers();
    void sendSetupResponse(nlohmann::json req);

    void closeStream();
    static void streamClosed(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData);

    void cleanup();


    NabtoDevicePtr device_;
    NabtoDeviceStream* stream_;
    SignalingStreamManagerPtr manager_;
    EventQueuePtr queue_;
    TrackEventCallback trackCb_;
    CheckAccessCallback accessCb_;
    DatachannelEventCallback datachannelCb_;
    bool isV2_ = false;
    NabtoDeviceFuture* future_;
    NabtoDeviceFuture* writeFuture_;

    size_t readLength_;
    uint32_t objectLength_;
    uint8_t* objectBuffer_;
    bool accepted_ = false;
    bool closed_ = false;
    bool closing_ = false;
    bool reading_ = false;

    uint8_t* writeBuf_ = NULL;
    std::queue<std::string> writeBuffers_;

    NabtoDeviceIceServersRequest* iceReq_;
    std::vector<WebrtcConnection::TurnServer> turnServers_;
    WebrtcConnectionPtr webrtcConnection_;
    SignalingStreamPtr self_;


    std::vector<MediaTrackPtr> deferredTracks_;


};


} // namespace
