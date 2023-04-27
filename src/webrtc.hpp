#pragma once

#include "webrtc_stream.hpp"

#include <nabto/nabto_device.h>

#include <rtc/rtc.hpp>

namespace nabto{

typedef bool (*check_access)(NabtoDeviceConnectionRef ref, const char* action, void* userData);

class NabtoWebrtc {
  public:
    NabtoWebrtc(NabtoDevice* dev, check_access checkAccess, void* userData);
    ~NabtoWebrtc();
    void run();

  private:
    NabtoDevice* device_;
    check_access accessCb_;
    void* accessUserData_;
    NabtoDeviceListener* streamListener_;
    NabtoDeviceFuture* streamListenFuture_;

    std::vector<WebrtcStreamWeakPtr> streams_;
};

} // namespace
