#include "track_negotiator.hpp"

namespace nabto {

    Repacketizer::Repacketizer(MediaTrackPtr track):
        track_(track)
    {

    }

    void Repacketizer::handlePacket(const uint8_t* buffer, size_t length)
    {
        track_->send(buffer, length);
    }

} // namespace
