#pragma once

#include "media_stream.hpp"
#include "rtp_client.hpp"

#include <rtc/rtc.hpp>

namespace nabto {

class RtpStream;

typedef std::shared_ptr<RtpStream> RtpStreamPtr;

typedef std::shared_ptr<rtc::PeerConnection> RtcPCPtr;

class RtpStream : public MediaStream,
    public std::enable_shared_from_this<RtpStream>
{
public:
    static RtpStreamPtr create(std::string trackId);
    RtpStream(std::string& trackId);
    ~RtpStream();

    /* Configure a video stream:
     * port: port to listen for UDP data on. Port+1 will be used to send data to if this is a 2-way stream.
     * matcher: codec matcher for this stream
     * host: if this is a 2-way stream, UDP data will be sent to this host.
     */
    void setVideoConf(uint16_t port, RtpCodec* matcher, std::string host = "127.0.0.1");

    /* Configure a audio stream:
     * port: port to listen for UDP data on. Port+1 will be used to send data to if this is a 2-way stream.
     * matcher: codec matcher for this stream
     * host: if this is a 2-way stream, UDP data will be sent to this host.
     */
    void setAudioConf(uint16_t port, RtpCodec* matcher, std::string host = "127.0.0.1");

    void stop() {
    }

    void addTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc, std::string trackId);

    void createTrack(std::shared_ptr<rtc::PeerConnection> pc);

    std::string getTrackId();

    void removeConnection(std::shared_ptr<rtc::PeerConnection> pc);

    std::string sdp();

private:
    std::string trackId_;
    RtpClientPtr videoClient_ = nullptr;
    RtpClientPtr audioClient_ = nullptr;
};

} // namespace

