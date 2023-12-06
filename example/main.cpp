#include <nabto/nabto_device_webrtc.hpp>
#include <event-queue/event_queue_impl.hpp>

int main(int argc, char** argv) {
    auto device = nabto::makeNabtoDevice();
    auto queue = nabto::EventQueueImpl::create();
    auto webrtc = nabto::NabtoDeviceWebrtc::create(queue, device);

}
