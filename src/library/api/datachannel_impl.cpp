#include "datachannel_impl.hpp"

namespace nabto {

DatachannelImpl::DatachannelImpl(const std::string& label)
: label_(label)
{

}

void DatachannelImpl::sendMessage(const uint8_t* buffer, size_t length, enum Datachannel::MessageType type)
{
    if (type == Datachannel::MESSAGE_TYPE_STRING) {
        std::vector<std::byte> vec((const std::byte*)buffer, (const std::byte*)buffer + length);
        auto msg = rtc::make_message(vec.begin(), vec.end(), rtc::Message::String);
        channel_->send(rtc::to_variant(*msg));
    } else if (type == Datachannel::MESSAGE_TYPE_BINARY) {
        std::vector<std::byte> vec((const std::byte*)buffer, (const std::byte*)buffer + length);
        auto msg = rtc::make_message(vec.begin(), vec.end(), rtc::Message::Binary);
        channel_->send(rtc::to_variant(*msg));
    } else {
        channel_->send((const std::byte*)buffer, length);
    }
}

void DatachannelImpl::setCloseCallback(std::function<void()> cb)
{
    closeCb_ = cb;
}

} // namespace
