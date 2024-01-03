#include <nabto/nabto_device_webrtc.hpp>

#include <rtc/rtc.hpp>

namespace nabto {

class MediaTrackImpl : public std::enable_shared_from_this<MediaTrackImpl> {
public:
    MediaTrackImpl(std::string& trackId, std::string& sdp);
    ~MediaTrackImpl();

    // API METHODS
    std::string getTrackId();
    std::string getSdp();
    void setSdp(std::string& sdp);
    bool send(const uint8_t* buffer, size_t length);
    void setReceiveCallback(MediaRecvCallback cb);
    void setCloseCallback(std::function<void()> cb);
    void setErrorState(enum MediaTrack::ErrorState state);
    void close();

    // INTERNAL METHODS
    void setRtcTrack(std::shared_ptr<rtc::Track> track);
    std::shared_ptr<rtc::Track> getRtcTrack() { return rtcTrack_; }
    void connectionClosed();
    void handleTrackMessage(rtc::message_ptr msg);
    enum MediaTrack::ErrorState getErrorState() { return state_; }
private:
    std::string trackId_;
    std::string sdp_;
    MediaRecvCallback recvCb_ = nullptr;
    std::function<void()> closeCb_ = nullptr;

    enum MediaTrack::ErrorState state_ = MediaTrack::ErrorState::OK;
    std::shared_ptr<rtc::Track> rtcTrack_ = nullptr;
};

} // namespace nabto
