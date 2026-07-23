#include <nabto/nabto_device_webrtc.hpp>

#include <rtc/rtc.hpp>

#include <mutex>

namespace nabto {

class DatachannelImpl : public std::enable_shared_from_this <DatachannelImpl> {
public:
    DatachannelImpl(const std::string& label);
    ~DatachannelImpl() {};

    void setMessageCallback(Datachannel::DatachannelMessageCallback cb)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        cb_ = cb;
    };
    void sendMessage(const uint8_t* buffer, size_t length, enum Datachannel::MessageType type);

    void setRtcChannel(std::shared_ptr<rtc::DataChannel> channel)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            channel_ = channel;
        }
        auto self = shared_from_this();
        channel->onMessage([self](rtc::binary data) {
            auto cb = self->messageCallback();
            if (cb) {
                cb(Datachannel::MessageType::MESSAGE_TYPE_BINARY, (uint8_t*)data.data(), data.size());
            }
        },
        [self](std::string data) {
            auto cb = self->messageCallback();
            if (cb) {
                cb(Datachannel::MessageType::MESSAGE_TYPE_STRING, (uint8_t*)data.data(), data.size());
            }
        });
    }

    void setCloseCallback(std::function<void()> cb);

    void connectionClosed()
    {
        std::function<void()> closeCb = nullptr;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closeCb = closeCb_;
            cb_ = nullptr;
            closeCb_ = nullptr;
            channel_ = nullptr;
        }
        if (closeCb) {
            closeCb();
        }
    }

    std::string getLabel() {
        return label_;
    }

private:
    Datachannel::DatachannelMessageCallback messageCallback()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return cb_;
    }

    std::string label_;
    // mutex_ protects channel_, cb_ and closeCb_ as they are accessed from
    // application threads, the event queue and libdatachannel callbacks.
    std::mutex mutex_;
    std::shared_ptr<rtc::DataChannel> channel_;
    Datachannel::DatachannelMessageCallback cb_;
    std::function<void()> closeCb_ = nullptr;

};

} // namespace nabto
