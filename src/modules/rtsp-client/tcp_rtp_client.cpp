#include "tcp_rtp_client.hpp"
#include <curl/curl.h>

#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <errno.h>
#include <cstring>

namespace nabto {

TcpRtpClientPtr TcpRtpClient::create(const TcpRtpClientConf& conf)
{
    // THIS IS CALLED FROM THE CURL WORKER THREAD!
    return std::make_shared<TcpRtpClient>(conf);
}

TcpRtpClient::TcpRtpClient(const TcpRtpClientConf& conf)
{
    NPLOGD << "TcpRtpClient constructor";
    curl_ = conf.curl;
    url_ = conf.url;
    videoNegotiator_ = conf.videoNegotiator;
    if (conf.videoRepack != nullptr) {
        videoRepack_ = conf.videoRepack;
    }
    audioNegotiator_ = conf.audioNegotiator;
    if (conf.audioRepack != nullptr) {
        audioRepack_ = conf.audioRepack;
    }
}

TcpRtpClient::~TcpRtpClient() {}


void TcpRtpClient::setConnection(NabtoDeviceConnectionRef ref, MediaTrackPtr videoTrack, MediaTrackPtr audioTrack)
{
    NPLOGD << "TcpRtpClient setConnection";
    if (videoTrack != nullptr) {
        videoTrack_ = videoTrack;
        auto sdp = videoTrack_->getSdp();
        rtc::Description::Media desc(sdp);
        auto pts = desc.payloadTypes();
        int pt = pts.empty() ? 0 : pts[0];
        const rtc::SSRC ssrc = videoNegotiator_->ssrc();
        videoRepacketizer_ = videoRepack_->createPacketizer(videoTrack_, ssrc, pt);
        videoSrcPt_ = videoNegotiator_->payloadType();
        videoDstPt_ = pt;
    }
    if (audioTrack != nullptr) {
        audioTrack_ = audioTrack;
        auto sdp = audioTrack_->getSdp();
        rtc::Description::Media desc(sdp);
        auto pts = desc.payloadTypes();
        int pt = pts.empty() ? 0 : pts[0];
        const rtc::SSRC ssrc = audioNegotiator_->ssrc();
        audioRepacketizer_ = audioRepack_->createPacketizer(audioTrack_, ssrc, pt);
        audioSrcPt_ = audioNegotiator_->payloadType();
        audioDstPt_ = pt;
    }
}

bool TcpRtpClient::prepareInterleave()
{
    auto curl = curl_->getCurl();
    CURLcode res = CURLE_OK;
    if ((res = curl_easy_setopt(curl, CURLOPT_INTERLEAVEFUNCTION, &TcpRtpClient::rtp_write)) != CURLE_OK ||
        (res = curl_easy_setopt(curl, CURLOPT_INTERLEAVEDATA, this)) != CURLE_OK) {
        NPLOGE << "Failed to install interleave callback: " << curl_easy_strerror(res);
        return false;
    }
    NPLOGD << "TcpRtpClient::prepareInterleave OK";
    return true;
}

void TcpRtpClient::run()
{
    NPLOGD << "TcpRtpClient run";
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = false;
    }

    auto curl = curl_->getCurl();

    // CURL_RTSPREQ_RECEIVE is the wrong primitive for continuous
    // interleaved RTP: libcurl's perform() waits for a server-side RTSP
    // response message that never arrives from streaming servers like
    // dolphin-rtsp-server, so the loop hangs at iter=1 with
    // INTERLEAVEFUNCTION never invoked (SC-4605).
    //
    // After PLAY succeeded the TCP socket is fully ours. Read it directly,
    // run the bytes through the same framer used by INTERLEAVEFUNCTION,
    // and let the existing RTCP RR write() path remain. This is the same
    // technique the RTCP-RR send code has been using already; we extend
    // it to the read side too. The INTERLEAVEFUNCTION installed by
    // prepareInterleave stays in place as a safety net in case any
    // pre-PLAY/early bytes happened to be delivered through it.
    curl_socket_t sockfd = CURL_SOCKET_BAD;
    CURLcode infoRes = curl_easy_getinfo(curl, CURLINFO_ACTIVESOCKET, &sockfd);
    if (infoRes != CURLE_OK || sockfd == CURL_SOCKET_BAD) {
        NPLOGE << "TcpRtpClient: cannot get active socket: "
               << curl_easy_strerror(infoRes) << " sock=" << sockfd;
        return;
    }
    NPLOGD << "TcpRtpClient run sockfd=" << sockfd;

    // libcurl leaves the socket in O_NONBLOCK; SO_RCVTIMEO is ignored on
    // non-blocking sockets and recv() returns EAGAIN immediately, hot-spinning
    // the loop (thousands of timeouts per second). Switch to blocking I/O
    // so SO_RCVTIMEO actually drives the wait.
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        NPLOGE << "fcntl(F_GETFL) failed: " << strerror(errno);
    } else if ((flags & O_NONBLOCK) &&
               fcntl(sockfd, F_SETFL, flags & ~O_NONBLOCK) == -1) {
        NPLOGE << "fcntl(F_SETFL ~O_NONBLOCK) failed: " << strerror(errno);
    }

    // SO_RCVTIMEO lets recv() return periodically so we can check stopped_
    // and act on pending RTCP without polling. SO_SNDTIMEO bounds the
    // RTCP RR write() below; if the camera's receive side stalls and our
    // send buffer fills, an unbounded write would block this thread and
    // stop() would hang waiting for stopped_ to be observed.
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 500 * 1000;  // 500 ms
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv)) < 0) {
        NPLOGE << "setsockopt(SO_RCVTIMEO) failed: " << strerror(errno);
        // Non-fatal; recv would just block longer.
    }
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO,
                   reinterpret_cast<const char*>(&tv), sizeof(tv)) < 0) {
        NPLOGE << "setsockopt(SO_SNDTIMEO) failed: " << strerror(errno);
        // Non-fatal; RTCP RR write may block longer.
    }

    uint8_t buf[8192];
    while (1) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopped_) {
                NPLOGD << "TcpRtpClient got stopped";
                break;
            }
        }

        ssize_t n = ::recv(sockfd, buf, sizeof(buf), 0);
        if (n < 0) {
            const int err = errno;
            if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK) {
                // SO_RCVTIMEO expired; loop and re-check stopped_.
            } else {
                NPLOGE << "TcpRtpClient recv failed: " << strerror(err);
                break;
            }
        } else if (n == 0) {
            NPLOGE << "TcpRtpClient: peer closed TCP connection";
            break;
        } else {
            processIncomingBytes(buf, static_cast<size_t>(n));
        }

        // Send any RTCP RR queued by drainInterleavedBuffer.
        bool pendingRtcp = false;
        size_t pendingRtcpLen = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (sendRtcp_) {
                sendRtcp_ = false;
                pendingRtcp = true;
                pendingRtcpLen = rtcpWriteLen_;
            }
        }
        if (pendingRtcp) {
            ssize_t ret = ::write(sockfd, rtcpWriteBuf_, pendingRtcpLen);
            if (ret < 0 || (size_t)ret < pendingRtcpLen) {
                // RTCP RR is best-effort. A short write or transient
                // EAGAIN/EINTR from SO_SNDTIMEO should not tear down the
                // receive loop; if the connection is actually dead, recv()
                // will surface it on the next iteration.
                NPLOGE << "Failed to write RTCP RR to TCP socket; dropping. ret: "
                       << ret << " expected: " << pendingRtcpLen
                       << " errno: " << strerror(errno);
            }
        }
    }
    NPLOGD << "TcpRtpClient run returning";
}

void TcpRtpClient::processIncomingBytes(const uint8_t* data, size_t len)
{
    std::lock_guard<std::mutex> lock(mutex_);
    recvBuf_.insert(recvBuf_.end(), data, data + len);
    drainInterleavedBuffer();
}


size_t TcpRtpClient::rtp_write(void* ptr, size_t size, size_t nmemb, void* userp)
{
    TcpRtpClient* self = (TcpRtpClient*)userp;
    size_t len = size * nmemb;
    if (len == 0) {
        return 0;
    }

    // libcurl's CURLOPT_INTERLEAVEFUNCTION delivers raw bytes from the
    // connection as they arrive: possibly a partial frame, possibly
    // multiple frames concatenated, possibly leftover RTSP response chunks
    // mixed with frame bytes. The previous implementation assumed one
    // complete frame per call and silently dropped data on mismatch, which
    // permanently desynced the libcurl RTSP state and caused
    // CURLE_RECV_ERROR on the next RECEIVE. See SC-4605.
    //
    // Buffer everything, then loop and emit complete frames from the
    // member buffer.
    {
        std::lock_guard<std::mutex> lock(self->mutex_);
        const uint8_t* in = static_cast<const uint8_t*>(ptr);
        self->recvBuf_.insert(self->recvBuf_.end(), in, in + len);
        self->drainInterleavedBuffer();
    }

    // Always tell libcurl we consumed everything; we've buffered it.
    return len;
}

void TcpRtpClient::drainInterleavedBuffer()
{
    // mutex_ must be held by caller.
    while (recvBuf_.size() >= 4) {
        // Resync: skip any non-'$' bytes (interleaved framing starts with
        // ASCII '$'). Locate the next '$' with memchr and erase in one
        // shot. std::vector::erase(begin()) shifts the whole tail, so
        // byte-by-byte erasure would be O(n^2) in the amount of garbage.
        if (recvBuf_.front() != '$') {
            const void* found = std::memchr(recvBuf_.data(), '$', recvBuf_.size());
            size_t skipped;
            if (found == nullptr) {
                skipped = recvBuf_.size();
                recvBuf_.clear();
            } else {
                skipped = static_cast<size_t>(
                    static_cast<const uint8_t*>(found) - recvBuf_.data());
                recvBuf_.erase(recvBuf_.begin(),
                               recvBuf_.begin() + skipped);
            }
            NPLOGE << "TcpRtpClient: skipped " << skipped
                   << " non-frame bytes before next '$' (resync)";
        }
        if (recvBuf_.size() < 4) {
            return;  // header not complete yet
        }

        const uint8_t channel = recvBuf_[1];
        const uint16_t dataLen =
            (static_cast<uint16_t>(recvBuf_[2]) << 8) |
            static_cast<uint16_t>(recvBuf_[3]);
        const size_t frameTotal = 4u + static_cast<size_t>(dataLen);

        if (recvBuf_.size() < frameTotal) {
            return;  // payload not complete yet
        }

        const uint8_t* payload = recvBuf_.data() + 4;

        if (channel == 0) {
            // Video RTP
            if (videoTrack_ != nullptr) {
                auto packets = videoRepacketizer_->handlePacket(
                    std::vector<uint8_t>(payload, payload + dataLen));
                for (auto& p : packets) {
                    videoTrack_->send(p.data(), p.size());
                }
            }
        } else if (channel == 2) {
            // Audio RTP
            if (audioTrack_ != nullptr) {
                auto packets = audioRepacketizer_->handlePacket(
                    std::vector<uint8_t>(payload, payload + dataLen));
                for (auto& p : packets) {
                    audioTrack_->send(p.data(), p.size());
                }
            }
        } else {
            // RTCP (channels 1 and 3: sender reports etc.). Build an RR
            // in rtcpWriteBuf_; the outer loop in run() will write it after
            // the current perform returns.
            //
            // Only build an RR from a sender report. Anything shorter than
            // an SR header (or from a non-SR packet type) we skip without
            // reading the payload; reinterpret_casting a too-short buffer
            // and then reading senderSSRC()/ntpTimestamp() would be an
            // out-of-bounds read.
            if (dataLen < sizeof(rtc::RtcpSr)) {
                NPLOGD << "TcpRtpClient: ignoring short RTCP frame on channel "
                       << static_cast<int>(channel) << " (" << dataLen
                       << " bytes)";
                recvBuf_.erase(recvBuf_.begin(),
                               recvBuf_.begin() + frameTotal);
                continue;
            }

            const char* buffer = reinterpret_cast<const char*>(payload);
            auto sr = reinterpret_cast<const rtc::RtcpSr*>(buffer);

            std::memset(rtcpWriteBuf_, 0, sizeof(rtcpWriteBuf_));
            rtcpWriteBuf_[0] = '$';
            rtcpWriteBuf_[1] = channel;

            rtc::RtcpRr* rr = reinterpret_cast<rtc::RtcpRr*>(rtcpWriteBuf_ + 4);
            rtc::RtcpReportBlock* rb = rr->getReportBlock(0);
            rb->preparePacket(sr->senderSSRC(), 0, 0, 0, 0, 0, sr->ntpTimestamp(), 0);
            rr->preparePacket(1, 1);

            // RFC 2326 §10.12: the framing length field is in NETWORK
            // byte order. The previous code wrote it host-order, which
            // silently corrupted the frame on little-endian targets (i.e.
            // every platform we ship on). See SC-4605.
            const size_t rtcpBytes = rr->header.lengthInBytes();
            uint16_t lenBe = htons(static_cast<uint16_t>(rtcpBytes));
            std::memcpy(&rtcpWriteBuf_[2], &lenBe, sizeof(lenBe));
            rtcpWriteLen_ = 4 + rtcpBytes;
            if (rtcpWriteLen_ > sizeof(rtcpWriteBuf_)) {
                NPLOGE << "RTCP RR overflows write buffer (" << rtcpWriteLen_
                       << " > " << sizeof(rtcpWriteBuf_) << "); dropping";
                rtcpWriteLen_ = 0;
            } else {
                sendRtcp_ = true;
            }
        }

        recvBuf_.erase(recvBuf_.begin(), recvBuf_.begin() + frameTotal);
    }
}


} // Namespace
