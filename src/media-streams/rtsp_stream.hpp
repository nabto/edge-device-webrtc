#pragma once

#include "media_stream.hpp"
#include "rtsp_client.hpp"

#include <rtc/rtc.hpp>

namespace nabto {

class RtspStream;

typedef std::shared_ptr<RtspStream> RtspStreamPtr;

typedef std::shared_ptr<rtc::PeerConnection> RtcPCPtr;

class RtspConnection
{
public:
    RtcPCPtr pc;
    RtspClientPtr client;
};


class RtspStream : public MediaStream,
                   public std::enable_shared_from_this<RtspStream>
{
public:
    static RtspStreamPtr create(std::string trackId, std::string& url);
    RtspStream(std::string& trackId, std::string& url);
    ~RtspStream();

    void stop() {
        // TODO: loop through conns and stop them
        connections_.clear();
    }

    void addTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc);

    std::shared_ptr<rtc::Track> createTrack(std::shared_ptr<rtc::PeerConnection> pc);

    std::string getTrackId();

    void removeConnection(std::shared_ptr<rtc::PeerConnection> pc);

private:

    static bool pcPtrComp(const RtcPCPtr& a, const RtcPCPtr& b) {
        if (a == b) return true;
        if (a && b) return a.get() == b.get();
        return false;
    };

    std::string trackId_;
    std::string url_;

    std::vector<RtspConnection> connections_;
};

} // namespace

