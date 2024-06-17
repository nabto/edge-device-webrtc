#pragma once
#include <media-streams/media_stream.hpp>
#include <track-negotiators/track_negotiator.hpp>
#include <rtp-packetizer/rtp_packetizer.hpp>

#include <iostream>
#include <memory>

#include <fstream>

namespace nabto {

class FifoFileClient;
typedef std::shared_ptr<FifoFileClient> FifoFileClientPtr;

class FifoTrack
{
public:
    MediaTrackPtr track;
    RtpPacketizerPtr packetizer = nullptr;

};

class FifoFileClientConf {
public:
    std::string trackId;
    std::string filePath;
    TrackNegotiatorPtr negotiator;
    RtpPacketizerFactoryPtr packetizer;
};

class FifoFileClient : public MediaStream, public std::enable_shared_from_this<FifoFileClient>
{
public:
    static FifoFileClientPtr create(const FifoFileClientConf& conf);
    FifoFileClient(const FifoFileClientConf& conf);
    ~FifoFileClient();

    bool isTrack(const std::string& trackId);
    void addConnection(NabtoDeviceConnectionRef ref, MediaTrackPtr media);
    void removeConnection(NabtoDeviceConnectionRef ref);
    bool matchMedia(MediaTrackPtr media);

    MediaTrackPtr createMedia(const std::string& trackId) {
        auto m = negotiator_->createMedia();
        m.addSSRC(negotiator_->ssrc(), trackId_);
        auto sdp = m.generateSdp();
        return MediaTrack::create(trackId, sdp);
    }

    TrackNegotiatorPtr getTrackNegotiator() { return negotiator_; }

private:
    void start();
    void stop();
    void doAddConnection(NabtoDeviceConnectionRef ref, FifoTrack track);
    static void fifoRunner(FifoFileClient* self);

    std::string trackId_;
    std::string filePath_;

    bool stopped_ = true;
    std::mutex mutex_;
    std::map<NabtoDeviceConnectionRef, FifoTrack> mediaTracks_;
    TrackNegotiatorPtr negotiator_;
    RtpPacketizerFactoryPtr packetizer_;
    std::thread thread_;

    int fd_;

};
} // namespace
