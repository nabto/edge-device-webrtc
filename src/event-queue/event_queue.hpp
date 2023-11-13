#pragma once

#include <memory>
#include <functional>

namespace nabto {

class EventQueue;
typedef std::shared_ptr<EventQueue> EventQueuePtr;

typedef std::function<void()> QueueEvent;

class EventQueue {
public:
    EventQueue() {}
    ~EventQueue() {}

    virtual void start() = 0;
    virtual void post(QueueEvent event) = 0;
    virtual void stop() = 0;

};


} // namespace
