#pragma once

#include <memory>
#include <functional>

namespace nabto2 {

class EventQueue;
typedef std::shared_ptr<EventQueue> EventQueuePtr;

typedef std::function<void()> QueueEvent;

class EventQueue {
public:
    EventQueue() {}
    ~EventQueue() {}

    virtual void post(QueueEvent event) = 0;
    virtual void addWork() = 0;
    virtual void removeWork() = 0;

};

class EventQueueWork {
public:
    EventQueueWork(EventQueuePtr queue) : queue_(queue)
    {
        queue_->addWork();
    }
    ~EventQueueWork()
    {
        queue_->removeWork();
    }
private:
    EventQueuePtr queue_;
};


} // namespace
