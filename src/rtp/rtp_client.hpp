#pragma once

#include <memory>

namespace nabto {

class RtpClient;

typedef std::shared_ptr<RtpClient> RtpClientPtr;

class RtpClient : public std::enable_shared_from_this<RtpClient>
{
public:

private:

};


} // namespace
