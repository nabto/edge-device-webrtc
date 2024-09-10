#include "nabto_device_webrtc_impl.hpp"

namespace nabto {

NabtoDeviceWebrtcImpl::NabtoDeviceWebrtcImpl(EventQueuePtr queue, NabtoDevicePtr device): queue_(queue), device_(device)
{
    ssm_ = SignalingStreamManager::create(device, queue);
}

NabtoDeviceWebrtcImpl::~NabtoDeviceWebrtcImpl()
{

}


void NabtoDeviceWebrtcImpl::start()
{
    ssm_->start();
}

void NabtoDeviceWebrtcImpl::stop()
{
    // TODO: impl stop()
}

bool NabtoDeviceWebrtcImpl::connectionAddMediaTracks(const std::string& webrtcConnectionId, const std::vector<MediaTrackPtr>& tracks)
{
    return ssm_->connectionAddMediaTracks(webrtcConnectionId, tracks);
}

void NabtoDeviceWebrtcImpl::setTrackEventCallback(TrackEventCallback cb)
{
    ssm_->setTrackEventCallback(cb);
}

void NabtoDeviceWebrtcImpl::setDatachannelEventCallback(DatachannelEventCallback cb)
{
    ssm_->setDatachannelEventCallback(cb);
}

void NabtoDeviceWebrtcImpl::setMetadataEventCallback(MetadataEventCallback cb)
{
    ssm_->setMetadataEventCallback(cb);
}

bool NabtoDeviceWebrtcImpl::connectionSendMetadata(std::string id, std::string metadata)
{
    return ssm_->connectionSendMetadata(id, metadata);
}

NabtoDeviceConnectionRef NabtoDeviceWebrtcImpl::getNabtoConnectionRef(std::string webrtcConnectionId)
{
    return ssm_->getNabtoConnectionRef(webrtcConnectionId);
}


void NabtoDeviceWebrtcImpl::setCheckAccessCallback(CheckAccessCallback cb)
{
    ssm_->setCheckAccessCallback(cb);
}


} // namespace nabto
