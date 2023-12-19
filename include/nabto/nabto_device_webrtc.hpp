#pragma once

#include <nabto/nabto_device.h>

#include <memory>
#include <functional>

namespace nabto {

class NabtoDeviceWebrtc;
class EventQueue;
class MediaTrack;

class NabtoDeviceWebrtcImpl;
typedef std::shared_ptr<NabtoDeviceWebrtcImpl> NabtoDeviceWebrtcImplPtr;
class MediaTrackImpl;
typedef std::shared_ptr<MediaTrackImpl> MediaTrackImplPtr;

typedef std::shared_ptr<NabtoDeviceWebrtc> NabtoDeviceWebrtcPtr;
typedef std::shared_ptr<EventQueue> EventQueuePtr;
typedef std::shared_ptr<MediaTrack> MediaTrackPtr;

typedef std::function<void()> QueueEvent;
typedef std::function<void(NabtoDeviceConnectionRef connRef, MediaTrackPtr track)> TrackEventCallback;
typedef std::function<bool(NabtoDeviceConnectionRef connRef, std::string action)> CheckAccessCallback;


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

/**
 * EventQueue to synchronize events into single thread.
 */
class EventQueue {
public:
    /**
     * Post an event from any thread to be run in the queue thread
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

class EventQueueWork {
public:
    /**
     * Create event queue Work to keep a queue alive even if it has no events.
     */
    EventQueueWork(EventQueuePtr queue);
    ~EventQueueWork();
private:
    EventQueuePtr queue_;

};


class MediaTrack {
public:
    /**
     * Create a media track to use in a `connectionAddMedias()` call
     */
    static MediaTrackPtr create(std::string& trackId, std::string& sdp);

    MediaTrack(std::string& trackId, std::string& sdp);

    /**
     * Get track ID of this track
    */
    std::string getTrackId();

    /**
     * Get current SDP for this track
    */
    std::string getSdp();

    /**
     * Set a new SDP for this track
    */
    void setSdp(std::string& sdp);

    /**
     * Send data on this track
    */
    bool send(const uint8_t* buffer, size_t length);

    /**
     * Set callback to be called when data is received on this track.
    */
    void setReceiveCallback(std::function<void(const uint8_t* buffer, size_t length)> cb);

    /**
     * Set callback to be called when this track is closed.
    */
    void setCloseCallback(std::function<void()> cb);

    void close();

    /**
     * Internal method
    */
    MediaTrackImplPtr getImpl();
private:
    MediaTrackImplPtr impl_;

};

class NabtoDeviceWebrtc {
public:
    /**
     * Create an instance of NabtoDeviceWebrtc.
     *
     * @param queue  The event queue to synchronize onto
     * @param device The Nabto Device
     * @return A smart pointer to the created instance
     */
    static NabtoDeviceWebrtcPtr create(EventQueuePtr queue, NabtoDevicePtr device );


    NabtoDeviceWebrtc(EventQueuePtr queue, NabtoDevicePtr device);
    ~NabtoDeviceWebrtc();

    // TODO: implement
    // void stop();

    /**
     * Add MediaTracks to a Nabto Connection. These tracks will be added to the WebRTC PeerConnection which will send an WebRTC Offer to the client to negotiate (or renegotiate if a connection is already established) the WebRTC connection.
    */
    bool connectionAddMedias(NabtoDeviceConnectionRef ref, std::vector<MediaTrackPtr>& tracks);

    /**
     * Set callback to be called when the Client has added a track to the PeerConnection. (ie. a WebRTC Offer was received)
     * The MediaTrack provided in this callback will have an SDP description containing all payload types offered by the client. The callback must remove any unsupported payload types from the SDP before returning. When the callback returns, the SDP of the MediaTrack will be used in the WebRTC Answer returned to the client.
    */
    void setTrackEventCallback(TrackEventCallback cb);

    /**
     * Set Callback to be called to check if some action is allowed by the current connection.
    */
    void setCheckAccessCallback(CheckAccessCallback cb);

private:
    NabtoDeviceWebrtcImplPtr impl_;
};

} // namespace nabto
