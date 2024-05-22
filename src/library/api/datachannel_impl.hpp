#include <nabto/nabto_device_webrtc.hpp>

#include <rtc/rtc.hpp>

namespace nabto {

class DatachannelImpl : public std::enable_shared_from_this <DatachannelImpl> {
public:
    DatachannelImpl(const std::string& label);
    ~DatachannelImpl() {};

    void setMessageCallback(DatachannelMessageCallback cb)
    {
        cb_ = cb;
    };
    void sendMessage(const uint8_t* buffer, size_t length);

    void setRtcChannel(std::shared_ptr<rtc::DataChannel> channel)
    {
        channel_ = channel;
        auto self = shared_from_this();
        channel_->onMessage([self](rtc::binary data) {
            self->cb_((uint8_t*)data.data(), data.size());
        },
        [self](std::string data) {
                self->cb_((uint8_t*)data.data(), data.size());
        });
    }
private:
    std::string label_;
    std::shared_ptr<rtc::DataChannel> channel_;
    DatachannelMessageCallback cb_;

};

} // namespace nabto

