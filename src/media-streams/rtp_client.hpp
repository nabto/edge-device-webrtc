#pragma once

#include "media_stream.hpp"
#include "rtp_track.hpp"

#include <sys/socket.h>
typedef int SOCKET;

#include <memory>
#include <thread>

namespace nabto {

class RtpClient;
typedef std::shared_ptr<RtpClient> RtpClientPtr;



class RtpCodec
{
public:
    enum Direction {
        SEND_ONLY, // Device only sends data
        RECV_ONLY, // Device only receives data
        SEND_RECV  // Device both sends and receives data
    };

    /**
     * match takes a media description and loops through the rtp map
     * For each map entry if:
     *    it matches: its payload type is used as return value
     *    it doesn't match: it is removed from the map
     **/
    virtual int match(rtc::Description::Media* media) = 0;

    /**
     * createMedia creates a media description of the proper type and returns it.
     * The caller will add ssrc and track ID to the returned description.
     */
    virtual rtc::Description::Media createMedia() = 0;

    // payload type used to create media
    virtual int payloadType() = 0;

    // SSRC of the media
    virtual int ssrc() = 0;

    // Direction of the created media
    virtual enum Direction direction() { return SEND_ONLY;}
};

class H264CodecMatcher : public RtpCodec
{
public:
    int match(rtc::Description::Media* media);
    rtc::Description::Media createMedia();
    int payloadType() {return 96;}
    int ssrc() { return 42; }
};

class OpusCodecMatcher : public RtpCodec
{
public:
    int match(rtc::Description::Media* media);
    rtc::Description::Media createMedia();
    int payloadType() { return 111; }
    int ssrc() { return 43; }
    enum Direction direction() { return SEND_RECV; }
};

class RtpClient : public MediaStream,
                  public std::enable_shared_from_this<RtpClient>
{
public:
    static RtpClientPtr create(std::string trackId);
    RtpClient(std::string& trackId);
    ~RtpClient();

    void addTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc);

    std::shared_ptr<rtc::Track> createTrack(std::shared_ptr<rtc::PeerConnection> pc);

    std::string getTrackId();

    void removeConnection(std::shared_ptr<rtc::PeerConnection> pc);

    void setVideoPort(uint16_t port) { videoPort_ = port; remotePort_ = port + 1; }
    void setVideoHost(std::string host) { videoHost_ = host; }
    void setRtpCodecMatcher(RtpCodec* matcher) {matcher_ = matcher;}


private:
    void start();
    void stop();
    static void rtpVideoRunner(RtpClient* self);
    static bool pcPtrComp(const RtcPCPtr& a, const RtcPCPtr& b) {
        if (a == b) return true;
        if (a && b) return a.get() == b.get();
        return false;
    };

    std::string trackId_;
    bool stopped_ = true;

    std::vector<RtpTrack> videoTracks_;
    uint16_t videoPort_ = 6000;
    uint16_t remotePort_ = 6002;
    std::string videoHost_ = "127.0.0.1";
    SOCKET videoRtpSock_ = 0;
    std::thread videoThread_;
    RtpCodec* matcher_;
    rtc::SSRC ssrc_ = 424242;

};


} // namespace
