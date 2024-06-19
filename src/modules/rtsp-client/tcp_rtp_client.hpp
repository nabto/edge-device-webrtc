#pragma once

#include <media-streams/media_stream.hpp>
#include <track-negotiators/track_negotiator.hpp>
#include <rtp-repacketizer/rtp_repacketizer.hpp>

#include <util/util.hpp>


namespace nabto {

class TcpRtpClient;
typedef std::shared_ptr<TcpRtpClient> TcpRtpClientPtr;

class TcpRtpClientConf {
public:
    CurlAsyncPtr curl;
    std::string url;
    TrackNegotiatorPtr videoNegotiator;
    TrackNegotiatorPtr audioNegotiator;
    RtpRepacketizerFactoryPtr videoRepack;
    RtpRepacketizerFactoryPtr audioRepack;
};

class TcpRtpClient : public std::enable_shared_from_this<TcpRtpClient>
{
public:
    static TcpRtpClientPtr create(const TcpRtpClientConf& conf);
    TcpRtpClient(const TcpRtpClientConf& conf);

    ~TcpRtpClient();

    void setConnection(NabtoDeviceConnectionRef ref, MediaTrackPtr videoTrack, MediaTrackPtr audioTrack);

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopped_ = true;
        }
        curl_->stop();
    }

    void run();

private:
    static size_t rtp_write(void* ptr, size_t size, size_t nmemb, void* userp);

    CurlAsyncPtr curl_;
    std::string url_;
    bool stopped_ = true;
    std::mutex mutex_;

    TrackNegotiatorPtr videoNegotiator_ = nullptr;
    RtpRepacketizerFactoryPtr videoRepack_ = RtpRepacketizerFactory::create();
    RtpRepacketizerPtr videoRepacketizer_ = nullptr;
    MediaTrackPtr videoTrack_ = nullptr;
    uint32_t videoSsrc_ = 0;
    int videoSrcPt_ = 0;
    int videoDstPt_ = 0;

    TrackNegotiatorPtr audioNegotiator_ = nullptr;
    RtpRepacketizerFactoryPtr audioRepack_ = RtpRepacketizerFactory::create();
    RtpRepacketizerPtr audioRepacketizer_ = nullptr;
    MediaTrackPtr audioTrack_ = nullptr;
    uint32_t audioSsrc_ = 0;
    int audioSrcPt_ = 0;
    int audioDstPt_ = 0;
};




} // namespace
