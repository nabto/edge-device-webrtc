#include "event_queue_impl.hpp"
#include <iostream>

namespace nabto {

EventQueueImpl::EventQueueImpl()
{

}

EventQueueImpl::~EventQueueImpl()
{

}

void EventQueueImpl::run()
{
    eventRunner();
}

void EventQueueImpl::post(QueueEvent event)
{
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push(event);
    cond_.notify_one();
}

void EventQueueImpl::addWork()
{
    std::lock_guard<std::mutex> lock(mutex_);
    workCount_++;
}

void EventQueueImpl::removeWork()
{
    std::lock_guard<std::mutex> lock(mutex_);
    workCount_--;
    cond_.notify_one(); // break the wait in case this was the last work
}

QueueEvent EventQueueImpl::pop()
{
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        // If no events AND (outstanding workers OR not stopped), wait for events
        if (events_.empty() && workCount_ > 0) {
            cond_.wait(lock);
        }
        // If we have an event, store it to be executed outside the lock
        if (!events_.empty()) {
            QueueEvent ev = events_.front();
            events_.pop();
            return ev;
        } else if (workCount_ < 1) {// Else if queue has no workers, stop
            return nullptr;
        }
        // work was removed which broke the wait, however, it was not the last work, and there was no events to handle, going back to wait
    }
}


void EventQueueImpl::eventRunner()
{
    while(true) {
        QueueEvent ev = pop();
        if (ev == nullptr) {
            return;
        }
        ev();
    }
}



} // namespace
