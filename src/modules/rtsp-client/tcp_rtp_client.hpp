#pragma once

#include <media-streams/media_stream.hpp>
#include <track-negotiators/track_negotiator.hpp>
#include <rtp-repacketizer/rtp_repacketizer.hpp>

#include <util/util.hpp>

#include <vector>


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

    /**
     * Install the libcurl interleave callback on the curl handle.
     *
     * This must be called BEFORE the PLAY request is performed, because
     * many embedded RTSP servers (e.g. dolphin-rtsp-server on cheap IPC
     * camera firmwares) start pushing interleaved RTP frames in the same
     * send() as the PLAY response. With no INTERLEAVEFUNCTION installed,
     * those frames are dropped/misrouted and libcurl's connection state
     * desyncs against the camera. See SC-4605.
     */
    bool prepareInterleave();

    void run();

private:
    static size_t rtp_write(void* ptr, size_t size, size_t nmemb, void* userp);

    /// Drain complete `$<ch><be16-len><payload>` frames from recvBuf_,
    /// dispatching each to the matching track. Tolerates partial frames
    /// (leftover stays in recvBuf_) and concatenated frames (loop until
    /// the buffer holds less than one complete frame).
    void drainInterleavedBuffer();

    /// Append incoming bytes to recvBuf_ and drain.
    /// Caller does NOT hold mutex_.
    void processIncomingBytes(const uint8_t* data, size_t len);

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

    char rtcpWriteBuf_[64];
    size_t rtcpWriteLen_ = 0;  ///< total bytes to write incl. $<ch><len> header
    bool sendRtcp_ = false;

    /// Byte buffer holding partially-received interleaved data between
    /// libcurl callback invocations. libcurl's INTERLEAVEFUNCTION delivers
    /// "whatever just arrived", not "exactly one frame", so we must
    /// reassemble.
    std::vector<uint8_t> recvBuf_;
};




} // namespace
