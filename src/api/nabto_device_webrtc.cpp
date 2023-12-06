#include <nabto/nabto_device_webrtc.hpp>
#include <api/nabto_device_webrtc_impl.hpp>

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

void NabtoDeviceWebrtc::stop()
{
    impl_->stop();
}


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



} // namespace nabto
