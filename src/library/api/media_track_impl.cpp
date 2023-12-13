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
