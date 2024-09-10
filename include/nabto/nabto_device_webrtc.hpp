#pragma once

#include <nabto/nabto_device.h>
#include <plog/Log.h>
#include <plog/Init.h>

#include <memory>
#include <functional>
#include <string>

#define NABTO_LOG_ID 42

#define NPLOGV PLOGV_(NABTO_LOG_ID)
#define NPLOGD PLOGD_(NABTO_LOG_ID)
#define NPLOGI PLOGI_(NABTO_LOG_ID)
#define NPLOGW PLOGW_(NABTO_LOG_ID)
#define NPLOGE PLOGE_(NABTO_LOG_ID)
#define NPLOGF PLOGF_(NABTO_LOG_ID)
#define NPLOGN PLOGN_(NABTO_LOG_ID)

namespace nabto {

class NabtoDeviceWebrtc;
class EventQueue;
class MediaTrack;
class Datachannel;

class NabtoDeviceWebrtcImpl;
typedef std::shared_ptr<NabtoDeviceWebrtcImpl> NabtoDeviceWebrtcImplPtr;
class MediaTrackImpl;
typedef std::shared_ptr<MediaTrackImpl> MediaTrackImplPtr;
class DatachannelImpl;
typedef std::shared_ptr<DatachannelImpl> DatachannelImplPtr;

typedef std::shared_ptr<NabtoDeviceWebrtc> NabtoDeviceWebrtcPtr;
typedef std::shared_ptr<EventQueue> EventQueuePtr;
typedef std::shared_ptr<MediaTrack> MediaTrackPtr;
typedef std::shared_ptr<Datachannel> DatachannelPtr;

typedef std::function<void()> QueueEvent;

/**
 * Track Event callback definition
 * @param connRef [in] The Nabto Connection the track event originates from
 * @param track [in]   The track created by this event
*/
typedef std::function<void(std::string webrtcConnectionId, MediaTrackPtr track)> TrackEventCallback;

/**
 * Datachannel Event callback definition
 * @param connRef [in] The Nabto Connection the datachannel event originates from
 * @param channel [in]   The datachannel created by this event
*/
typedef std::function<void(std::string webrtcConnectionId, DatachannelPtr channel)> DatachannelEventCallback;


typedef std::function<void(std::string webrtcConnectionId, std::string metadata)> MetadataEventCallback;


/**
 * Check access callback invoked when an access decision must be made.
 *
 * @param connRef [in] The Nabto Connection the requiring the access
 * @param action [in]  The action being requested
 * @return True if the action should be allowed
*/
typedef std::function<bool(NabtoDeviceConnectionRef ref, const std::string& action)> CheckAccessCallback;

/**
 * Callback invoked when a media track has data available.
 *
 * @param buffer [in] Data buffer received
 * @param length [in] Length of data buffer
 */
typedef std::function<void(uint8_t* buffer, size_t length)> MediaRecvCallback;


/**
 * Smart pointer handling lifetime of the NabtoDevice context to ensure it is not freed until all components has freed their resources.
 */
typedef std::shared_ptr<NabtoDevice> NabtoDevicePtr;

template< typename T >
struct NabtoDeviceDeleter {
    void operator()(T* device) {
        nabto_device_free(device);
    }
};

static NabtoDevicePtr makeNabtoDevice() {
    return std::shared_ptr<NabtoDevice>(nabto_device_new(),
        NabtoDeviceDeleter<NabtoDevice>());
}


static void initLogger(enum plog::Severity severity = plog::Severity::none, plog::IAppender* appender = NULL) {
    plog::init<NABTO_LOG_ID>(severity, appender);
    return;
}

/**
 * EventQueue to synchronize events into single thread.
 */
class EventQueue {
public:
    /**
     * Post an event from any thread to be run in the queue thread
     *
     * @param event [in] The event to post to the queue
     */
    virtual void post(QueueEvent event) = 0;

    /**
     * Add work to the queue. The Event Queue must not stop running while it has work (if it has work but no events, it must idle).
     */
    virtual void addWork() = 0;

    /**
     *  Remove work from the queue. Once all work has been removed, and it has no events to run, it can stop.
     */
    virtual void removeWork() = 0;

};

/**
 * EventQueueWork used to keep EventQueue alive when there are no events to handle.
 */
class EventQueueWork {
public:
    /**
     * Create event queue Work to keep a queue alive even if it has no events. This calls `addWork()` on the event queue
     *
     * @param queue [in] the queue to this work should be added to
     */
    EventQueueWork(EventQueuePtr queue);

    /**
     * Destroy event queue work. This calls `removeWork()` on the event queue.
     *
     */
    ~EventQueueWork();

private:
    EventQueuePtr queue_;

};

/**
 * A MediaTrack represents a WebRTC track, and is used to send or receive media data on.
 */
class MediaTrack {
public:
    /**
     * Error states to be set on a media track to inform the client of failures.
     */
    enum ErrorState {
        OK,
        ACCESS_DENIED,
        UNKNOWN_TRACK_ID,
        INVALID_CODECS,
        UNKNOWN_ERROR
    };

    /**
     * Create a media track to use in a `connectionAddMediaTracks()` call.
     *
     * @param trackId [in]  Identifier to set for the track
     * @param sdp [in]      SDP string defining the track
     * @return smart pointer to the created track
     */
    static MediaTrackPtr create(const std::string& trackId, const std::string& sdp);

    MediaTrack(const std::string& trackId, const std::string& sdp);
    ~MediaTrack();

    /**
     * Get track ID for this track
     *
     * @return track ID for this track
    */
    std::string getTrackId();

    /**
     * Get current SDP for this track
     *
     * @return SDP for this track
    */
    std::string getSdp();

    /**
     * Set a new SDP for this track.
     *
     * If the client adds a media track, the track is reveived in a track event. In that case, the track SDP may contain multiple codecs supported by the client. After getting the SDP with `getSdp()` and removing codecs not supported by the device, this function is used to the resulting SDP.
     *
     * @param sdp [in] The SDP string to set
    */
    void setSdp(const std::string& sdp);

    /**
     * Send data on this track
     *
     * @param buffer [in] Data buffer to send
     * @param length [in] Length of data buffer
     * @return True iff the RTC track is open and ready to send
    */
    bool send(const uint8_t* buffer, size_t length);

    /**
     * Set callback to be called when data is received on this track.
     *
     * @param cb [in] Callback to set
    */
    void setReceiveCallback(MediaRecvCallback cb);

    /**
     * Set callback to be called when this track is closed.
     *
     * @param cb [in] Callback to set
    */
    void setCloseCallback(std::function<void()> cb);


    /**
     * Set error state of the track. non-OK errors will be included in signaling message metadata returned to the client.
     *
     * @param state [in] Error state to set
    */
    void setErrorState(enum ErrorState state);

    /*
     * Internal method
    */
    MediaTrackImplPtr getImpl();
private:
    MediaTrackImplPtr impl_;

};

/**
 * This class represents a WebRTC DataChannel for sending and receiving messages.
 */
class Datachannel {
public:
    /**
     * Message types when sending/receiving datachannel messages
     */
    enum MessageType {
        MESSAGE_TYPE_BINARY,
        MESSAGE_TYPE_STRING
    };

    /**
     * Callback invoked when a datachannel has data available.
     *
     * @param type [in]   Type of message received
     * @param buffer [in] Data buffer received
     * @param length [in] Length of data buffer
     */
    typedef std::function<void(enum MessageType type, uint8_t* buffer, size_t length)> DatachannelMessageCallback;

    static DatachannelPtr create(const std::string& label);
    Datachannel(const std::string& label);
    ~Datachannel();

    /**
     * Get the label of the datachannel
     *
     * @return The label as a string
     */
    std::string getLabel();

    /**
     * Set callback to be invoked when a message is received on the datachannel.
     *
     * @param cb [in] Callback to set
    */
    void setMessageCallback(DatachannelMessageCallback cb);

    /**
     * Send a message on this datachannel
     *
     * @param buffer [in] Data buffer to send
     * @param length [in] Length of data buffer
     * @param type [in]   Type of message to send
     */
    void sendMessage(const uint8_t* buffer, size_t length, enum MessageType type = MESSAGE_TYPE_BINARY);

    /**
     * Set callback to be called when this track is closed.
     *
     * @param cb [in] Callback to set
    */
    void setCloseCallback(std::function<void()> cb);


    /*
     * Internal method
    */
    DatachannelImplPtr getImpl();
private:
    DatachannelImplPtr impl_;

};

/**
 * Main Webrtc Device context to handle WebRTC connections
 */
class NabtoDeviceWebrtc {
public:
    /**
     * Create an instance of NabtoDeviceWebrtc.
     *
     * This class does not have a close method. Instead, this will close gracefully when `nabto_device_stop()` is called on the underlying NabtoDevice context.
     *
     * @param queue  The event queue to synchronize onto
     * @param device The Nabto Device
     * @return A smart pointer to the created instance
     */
    static NabtoDeviceWebrtcPtr create(EventQueuePtr queue, NabtoDevicePtr device );


    NabtoDeviceWebrtc(EventQueuePtr queue, NabtoDevicePtr device);
    ~NabtoDeviceWebrtc();

    /**
     * Add MediaTracks to a Nabto Connection. These tracks will be added to the WebRTC PeerConnection which will send an WebRTC Offer to the client to negotiate (or renegotiate if a connection is already established) the WebRTC connection.
     *
     * To be able to negotiate the WebRTC connection, the client must first have established a Signaling Stream.
     *
     * @param ref [in] The Nabto Connection to add the media tracks to
     * @param tracks [in] List of tracks to add
     * @returns False if the Nabto Connection referenced does not have a Signaling Stream open
    */
    bool connectionAddMediaTracks(const std::string& webrtcConnectionId, const std::vector<MediaTrackPtr>& tracks);

    /**
     * Set callback to be called when the Client has added a track to the PeerConnection. (ie. a WebRTC Offer containing new tracks was received)
     *
     * The MediaTrack provided in this callback will have an SDP description containing all payload types offered by the client. The callback must remove any unsupported payload types from the SDP before returning. When the callback returns, the SDP of the MediaTrack will be used in the WebRTC Answer returned to the client.
     * If the callback is not able to properly handle the track, it must use `track->setErrorState()` to make the track fail on the client side.
     *
     * @param cb [in] The callback to set
    */
    void setTrackEventCallback(TrackEventCallback cb);


    /**
     * Set callback to be called when the Client has added a datachannel to the PeerConnection.
     *
     * @param cb [in] The callback to set
    */
    void setDatachannelEventCallback(DatachannelEventCallback cb);


    bool connectionSendMetadata(std::string id, std::string metadata);

    void setMetadataEventCallback(MetadataEventCallback cb);

    NabtoDeviceConnectionRef getNabtoConnectionRef(std::string webrtcConnectionId);

    /**
     * Set Callback to be called to check if some action is allowed by the current connection.
     *
     * When the callback is invoked, it must return true if the action is allowed otherwise it must return false. If no callback is set, all actions are denied.
     *
     * Possible actions are:
     * "Webrtc:Signaling"
     * "Webrtc:GetInfo"
     *
     * @param cb [in] The callback to set
     */
    void setCheckAccessCallback(CheckAccessCallback cb);

    /**
     * Get a string representation of the Nabto Device WebRTC library version.
     *
     * This returns the version of the WebRTC library, not the underlying Nabto Embedded SDK. For that use `nabto_device_version()`
     *
     * @returns String representation of the library version
     */
    static const std::string version();


private:
    NabtoDeviceWebrtcImplPtr impl_;
};

} // namespace nabto
