#include <nabto/nabto_device.h>
#include <nabto/nabto_device_webrtc.hpp>
#include <stdio.h>

#include <iostream>

int main() {
    printf("Nabto version %s\n", nabto_device_version());
    std::cout << "Nabto WebRTC Versoin" << nabto::NabtoDeviceWebrtc::version() << std::endl;
}
