#pragma once

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


class RtspStream : public std::enable_shared_from_this<RtspStream>
{
public:
    static RtspStreamPtr create(std::string trackId, std::string& url);
    RtspStream(std::string& trackId, std::string& url);
    ~RtspStream();

    void stop() {
        for (auto c : connections_) {
            c.client->stop();
        }
        connections_.clear();
    }

    void addTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc, std::string trackId);

    void createTrack(std::shared_ptr<rtc::PeerConnection> pc);

    std::string getTrackId();

    void removeConnection(std::shared_ptr<rtc::PeerConnection> pc);

    // TODO: impl sdp();
    std::string sdp() {return "";};


private:

    static bool pcPtrComp(const RtcPCPtr& a, const RtcPCPtr& b) {
        if (a == b) return true;
        if (a && b) return a.get() == b.get();
        return false;
    };

    std::string trackId_;
    std::string url_;

    std::mutex mutex_;
    size_t counter_ = 0;

    std::vector<RtspConnection> connections_;
};

} // namespace

