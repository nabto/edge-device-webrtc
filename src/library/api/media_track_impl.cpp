#include "media_track_impl.hpp"

namespace nabto {

MediaTrackImpl::MediaTrackImpl(std::string& trackId, std::string& sdp)
: trackId_(trackId), sdp_(sdp)
{

}

MediaTrackImpl::~MediaTrackImpl()
{

}


std::string MediaTrackImpl::getTrackId()
{
    return trackId_;
}

std::string MediaTrackImpl::getSdp()
{
    return sdp_;
}

void MediaTrackImpl::setSdp(std::string& sdp)
{
    sdp_ = sdp;
}

bool MediaTrackImpl::send(const uint8_t* buffer, size_t length)
{
    if (rtcTrack_ && rtcTrack_->isOpen()) {
        rtcTrack_->send(reinterpret_cast<const rtc::byte*>(buffer), length);
        return true;
    }
    return false;
}

void MediaTrackImpl::setReceiveCallback(std::function<void(const uint8_t* buffer, size_t length)> cb)
{
    recvCb_ = cb;
}

void MediaTrackImpl::setCloseCallback(std::function<void()> cb)
{
    closeCb_ = cb;
}

void MediaTrackImpl::close()
{

}

void MediaTrackImpl::connectionClosed()
{

}



} // namespace
