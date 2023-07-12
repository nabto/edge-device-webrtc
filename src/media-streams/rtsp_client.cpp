#include "rtsp_client.hpp"

namespace nabto {

RtspClientPtr RtspClient::create(std::string trackId, std::string& url)
{

}

RtspClient::RtspClient(std::string& trackId, std::string& url)
{

}

RtspClient::~RtspClient()
{

}


void RtspClient::addVideoTrack(RtcTrackPtr track, RtcPCPtr pc)
{

}

void RtspClient::addAudioTrack(RtcTrackPtr track, RtcPCPtr pc)
{

}

void RtspClient::removeConnection(RtcPCPtr pc)
{

}

std::string RtspClient::getVideoTrackId()
{

}

std::string RtspClient::getAudioTrackId()
{

}


} // namespace
