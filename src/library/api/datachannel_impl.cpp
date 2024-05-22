#include "datachannel_impl.hpp"

namespace nabto {

DatachannelImpl::DatachannelImpl(const std::string& label)
: label_(label)
{

}

void DatachannelImpl::sendMessage(const uint8_t* buffer, size_t length)
{
    channel_->send((const std::byte*)buffer, length);
}

} // namespace
