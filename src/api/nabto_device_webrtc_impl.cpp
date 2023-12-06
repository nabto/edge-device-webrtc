#include "nabto_device_webrtc_impl.hpp"

namespace nabto {

NabtoDeviceWebrtcImpl::NabtoDeviceWebrtcImpl(EventQueuePtr queue, NabtoDevicePtr device): queue_(queue), device_(device)
{

}

NabtoDeviceWebrtcImpl::~NabtoDeviceWebrtcImpl()
{

}


void NabtoDeviceWebrtcImpl::start()
{

}

void NabtoDeviceWebrtcImpl::stop()
{

}

bool NabtoDeviceWebrtcImpl::connectionAddMedias(NabtoDeviceConnectionRef ref, std::vector<MediaTrackPtr>& tracks)
{
    return false;
}

void NabtoDeviceWebrtcImpl::setTrackEventCallback(TrackEventCallback cb)
{

}

void NabtoDeviceWebrtcImpl::setCheckAccessCallback(CheckAccessCallback cb)
{

}


} // namespace nabto
