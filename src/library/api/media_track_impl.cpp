#include "media_track_impl.hpp"

namespace nabto {

MediaTrackImpl::MediaTrackImpl(const std::string& trackId, const std::string& sdp)
: trackId_(trackId), sdp_(sdp)
{

}

MediaTrackImpl::~MediaTrackImpl()
{
    std::cout << "MediaTrackImpl destructor" << std::endl;
}


std::string MediaTrackImpl::getTrackId()
{
    return trackId_;
}

std::string MediaTrackImpl::getSdp()
{
    return sdp_;
}

void MediaTrackImpl::setSdp(const std::string& sdp)
{
    sdp_ = sdp;
    if (sdp_[0] == 'm' && sdp_[1] == '=') {
        sdp_ = sdp_.substr(2);
    }
    if (rtcTrack_) {
        rtc::Description::Media media(sdp_);
        rtcTrack_->setDescription(media);
    }
}

bool MediaTrackImpl::send(const uint8_t* buffer, size_t length)
{
    if (rtcTrack_ && rtcTrack_->isOpen()) {
        rtcTrack_->send(reinterpret_cast<const rtc::byte*>(buffer), length);
        return true;
    }
    return false;
}

void MediaTrackImpl::setReceiveCallback(MediaRecvCallback cb)
{
    recvCb_ = cb;
}

void MediaTrackImpl::setCloseCallback(std::function<void()> cb)
{
    closeCb_ = cb;
}

void MediaTrackImpl::setErrorState(enum MediaTrack::ErrorState state) {
    state_ = state;
}

void MediaTrackImpl::close()
{

}

void MediaTrackImpl::connectionClosed()
{
    if (closeCb_) {
        closeCb_();
    }
    recvCb_ = nullptr;
    closeCb_ = nullptr;
}

void MediaTrackImpl::setRtcTrack(std::shared_ptr<rtc::Track> track) {
    rtcTrack_ = track;
    sdp_ = track->description().generateSdp();
}

void MediaTrackImpl::handleTrackMessage(rtc::message_ptr msg)
{
    if (recvCb_ != nullptr && msg->type == rtc::Message::Binary) {
        rtc::byte* data = msg->data();
        uint8_t* buf = reinterpret_cast<uint8_t*>(msg->data());
        recvCb_(buf, msg->size());
    }
}

} // namespace
