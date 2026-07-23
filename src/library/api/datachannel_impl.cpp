#include "datachannel_impl.hpp"

namespace nabto {

DatachannelImpl::DatachannelImpl(const std::string& label)
: label_(label)
{

}

void DatachannelImpl::sendMessage(const uint8_t* buffer, size_t length, enum Datachannel::MessageType type)
{
    auto channel = channel_;
    if (channel == nullptr || !channel->isOpen()){
        return;
    }
    try {
        if (type == Datachannel::MESSAGE_TYPE_STRING) {
            std::vector<std::byte> vec((const std::byte*)buffer, (const std::byte*)buffer + length);
            auto msg = rtc::make_message(vec.begin(), vec.end(), rtc::Message::String);
            channel->send(rtc::to_variant(*msg));
        } else if (type == Datachannel::MESSAGE_TYPE_BINARY) {
            std::vector<std::byte> vec((const std::byte*)buffer, (const std::byte*)buffer + length);
            auto msg = rtc::make_message(vec.begin(), vec.end(), rtc::Message::Binary);
            channel->send(rtc::to_variant(*msg));
        } else {
            channel->send((const std::byte*)buffer, length);
        }
    } catch (std::exception& ex) {
        // The channel can die between the isOpen() check and the send, eg. if
        // the client closes the connection while we are sending. The message
        // is lost, just like a message sent moments later would be.
        NPLOGD << "Failed to send datachannel message: " << ex.what();
    }
}

void DatachannelImpl::setCloseCallback(std::function<void()> cb)
{
    closeCb_ = cb;
}

} // namespace
