#include <nabto/nabto_device_webrtc.hpp>
#include <api/nabto_device_webrtc_impl.hpp>
#include <api/media_track_impl.hpp>
#include <api/datachannel_impl.hpp>

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


bool NabtoDeviceWebrtc::connectionAddMediaTracks(NabtoDeviceConnectionRef ref, const std::vector<MediaTrackPtr>& tracks)
{
    return impl_->connectionAddMediaTracks(ref, tracks);
}


void NabtoDeviceWebrtc::setTrackEventCallback(TrackEventCallback cb)
{
    impl_->setTrackEventCallback(cb);
}

void NabtoDeviceWebrtc::setDatachannelEventCallback(DatachannelEventCallback cb)
{
    impl_->setDatachannelEventCallback(cb);
}

void NabtoDeviceWebrtc::setCheckAccessCallback(CheckAccessCallback cb)
{
    impl_->setCheckAccessCallback(cb);
}


MediaTrackPtr MediaTrack::create(const std::string& trackId, const std::string& sdp)
{
    return std::make_shared<MediaTrack>(trackId, sdp);
}

MediaTrack::MediaTrack(const std::string& trackId, const std::string& sdp)
: impl_(std::make_shared<MediaTrackImpl>(trackId, sdp)) { }

MediaTrack::~MediaTrack() {
}

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


void MediaTrack::setSdp(const std::string& sdp)
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

void MediaTrack::setErrorState(enum MediaTrack::ErrorState state)
{
    return impl_->setErrorState(state);
}

DatachannelPtr Datachannel::create(const std::string& label)
{
    return  std::make_shared<Datachannel>(label);
}

Datachannel::Datachannel(const std::string& label)
    : impl_(std::make_shared<DatachannelImpl>(label))
{
}

Datachannel::~Datachannel()
{
}

void Datachannel::setMessageCallback(DatachannelMessageCallback cb)
{
    return impl_->setMessageCallback(cb);
}

void Datachannel::sendMessage(const uint8_t* buffer, size_t length, enum MessageType type)
{
    return impl_->sendMessage(buffer, length, type);
}

DatachannelImplPtr Datachannel::getImpl()
{
    return impl_;
}

EventQueueWork::EventQueueWork(EventQueuePtr queue) : queue_(queue)
{
    queue_->addWork();
}
EventQueueWork::~EventQueueWork()
{
    queue_->removeWork();
}



} // namespace nabto
