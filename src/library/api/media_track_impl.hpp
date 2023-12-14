#include <nabto/nabto_device_webrtc.hpp>

#include <rtc/rtc.hpp>

namespace nabto {

class MediaTrackImpl {
public:
    MediaTrackImpl(std::string& trackId, std::string& sdp);
    ~MediaTrackImpl();

    // API METHODS
    std::string getTrackId();
    std::string getSdp();
    void setSdp(std::string& sdp);
    bool send(const uint8_t* buffer, size_t length);
    void setReceiveCallback(std::function<void(const uint8_t* buffer, size_t length)> cb);
    void setCloseCallback(std::function<void()> cb);
    void close();

    // INTERNAL METHODS
    void setRtcTrack(std::shared_ptr<rtc::Track> track) { rtcTrack_ = track; }
    std::shared_ptr<rtc::Track> getRtcTrack() { return rtcTrack_; }
    void connectionClosed();
private:
    std::string trackId_;
    std::string sdp_;
    std::function<void(const uint8_t* buffer, size_t length)> recvCb_ = nullptr;
    std::function<void()> closeCb_ = nullptr;

    std::shared_ptr<rtc::Track> rtcTrack_ = nullptr;
};

} // namespace nabto
