#pragma once

#include <rtc/rtc.hpp>

#include <memory>
#include <functional>

namespace nabto {

class MediaStream;
typedef std::shared_ptr<MediaStream> MediaStreamPtr;

class MediaStream {
public:
    MediaStream() {}
    ~MediaStream() {}
    virtual void addVideoTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc) = 0;
    virtual void addAudioTrack(std::shared_ptr<rtc::Track> track, std::shared_ptr<rtc::PeerConnection> pc) = 0;
    virtual void removeConnection(std::shared_ptr<rtc::PeerConnection> pc) = 0;
    virtual std::string getVideoTrackId() = 0;
    virtual std::string getAudioTrackId() = 0;


    // virtual void handleVideoData(uint8_t* buffer, size_t len);
    // virtual void handleAudioData(uint8_t* buffer, size_t len);
    // virtual void addVideoSink(std::function<void(uint8_t* buffer, size_t len)> dataHandler);
    // virtual void addAudioSink(std::function<void(uint8_t* buffer, size_t len)> dataHandler);
};


} // namespace
