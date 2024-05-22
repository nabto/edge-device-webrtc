#include <nabto/nabto_device_webrtc.hpp>

#include <rtc/rtc.hpp>

namespace nabto {

class DatachannelImpl : public std::enable_shared_from_this <DatachannelImpl> {
public:
    DatachannelImpl(const std::string& label);
    ~DatachannelImpl() {};

    void setMessageCallback() {};
    void sendMessage(const uint8_t* buffer, size_t length);

    void setRtcChannel(std::shared_ptr<rtc::DataChannel> channel)
    {
        channel_ = channel;
    }
private:
    std::string label_;
    std::shared_ptr<rtc::DataChannel> channel_;

};

} // namespace nabto

