#pragma once

#include <memory>

namespace nabto {

class RtspClient;

typedef std::shared_ptr<RtspClient> RtspClientPtr;

class RtspClient : public std::enable_shared_from_this<RtspClient>
{
public:

private:

};


} // namespace
