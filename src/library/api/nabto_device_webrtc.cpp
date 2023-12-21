#include <nabto/nabto_device_webrtc.hpp>
#include <api/nabto_device_webrtc_impl.hpp>
#include <api/media_track_impl.hpp>

namespace nabto {

NabtoDeviceWebrtcPtr NabtoDeviceWebrtc::create(EventQueuePtr queue, NabtoDevicePtr device)
{
    auto ptr = std::make_shared<NabtoDeviceWebrtc>(queue, device);
    return ptr;
}

NabtoDeviceWebrtc::NabtoDeviceWebrtc(EventQueuePtr queue, NabtoDevicePtr device)
{
    impl_ = std::make_shared<NabtoDeviceWebrtcImpl>(queue, device);
    if (impl_) {
        impl_->start();
    }
}

NabtoDeviceWebrtc::~NabtoDeviceWebrtc()
{

}

// void NabtoDeviceWebrtc::stop()
// {
//     impl_->stop();
// }


bool NabtoDeviceWebrtc::connectionAddMedias(NabtoDeviceConnectionRef ref, std::vector<MediaTrackPtr>& tracks)
{
    return impl_->connectionAddMedias(ref, tracks);
}


void NabtoDeviceWebrtc::setTrackEventCallback(TrackEventCallback cb)
{
    impl_->setTrackEventCallback(cb);
}


void NabtoDeviceWebrtc::setCheckAccessCallback(CheckAccessCallback cb)
{
    impl_->setCheckAccessCallback(cb);
}


MediaTrackPtr MediaTrack::create(std::string& trackId, std::string& sdp)
{
    return std::make_shared<MediaTrack>(trackId, sdp);
}

MediaTrack::MediaTrack(std::string& trackId, std::string& sdp)
: impl_(std::make_shared<MediaTrackImpl>(trackId, sdp)) { }

MediaTrackImplPtr MediaTrack::getImpl()
{
    return impl_;
}

std::string MediaTrack::getTrackId()
{
    return impl_->getTrackId();
}

std::string MediaTrack::getSdp()
{
    return impl_->getSdp();
}


void MediaTrack::setSdp(std::string& sdp)
{
    return impl_->setSdp(sdp);
}


bool MediaTrack::send(const uint8_t* buffer, size_t length)
{
    return impl_->send(buffer, length);
}


void MediaTrack::setReceiveCallback(MediaRecvCallback cb)
{
    return impl_->setReceiveCallback(cb);
}


void MediaTrack::setCloseCallback(std::function<void()> cb)
{
    return impl_->setCloseCallback(cb);
}


void MediaTrack::close()
{
    return impl_->close();
}


} // namespace nabto
