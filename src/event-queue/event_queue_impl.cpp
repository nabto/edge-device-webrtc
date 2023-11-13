#include "event_queue_impl.hpp"
#include <iostream>

namespace nabto {

EventQueueImpl::EventQueueImpl()
{

}

EventQueueImpl::~EventQueueImpl()
{

}

void EventQueueImpl::start()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (stopped_) {
        stopped_ = false;
        eventThread_ = std::thread(eventRunner, this);
    }
}

void EventQueueImpl::post(QueueEvent event)
{
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push(event);
    cond_.notify_one();
}

void EventQueueImpl::stop()
{
    bool shouldJoin = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (eventThread_.joinable() && !stopped_) {
            shouldJoin = true;
        }
        stopped_ = true;
        cond_.notify_one();
    }
    if (shouldJoin) {
        eventThread_.join();
    }
}

QueueEvent EventQueueImpl::pop()
{
    std::unique_lock<std::mutex> lock(mutex_);
    // If no events, wait for events
    if (events_.empty()) {
        std::cout << "no events, waiting" << std::endl;
        cond_.wait(lock);
    }
    // If queue was stopped, return
    if (stopped_) {
        std::cout << "wait done, was stopped, returning" << std::endl;
        return nullptr;
    }
    // If we have an event, store it to be executed outside the lock
    if (!events_.empty()) {
        QueueEvent ev = events_.front();
        events_.pop();
        std::cout << "wait done, event popped" << std::endl;
        return ev;
    }
    // This should not happen!
    std::cout << "Queue event was popped but queue was empty" << std::endl;
    return nullptr;
}


void EventQueueImpl::eventRunner(EventQueueImpl* self)
{
    while(true) {
        QueueEvent ev = self->pop();
        if (ev == nullptr) {
            return;
        }
        ev();
    }
}


} // namespace
