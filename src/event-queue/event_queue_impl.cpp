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
    std::cout << "work added: " << workCount_ << std::endl;
}

void EventQueueImpl::removeWork()
{
    std::lock_guard<std::mutex> lock(mutex_);
    workCount_--;
    std::cout << "work removed: " << workCount_ << std::endl;
    cond_.notify_one(); // break the wait in case this was the last work
}

QueueEvent EventQueueImpl::pop()
{
    std::unique_lock<std::mutex> lock(mutex_);
    // If no events AND (outstanding workers OR not stopped), wait for events
    if (events_.empty() && workCount_ > 0) {
        std::cout << "no events, waiting. Work: " << workCount_ << std::endl;
        cond_.wait(lock);
    }
    // If we have an event, store it to be executed outside the lock
    if (!events_.empty()) {
        QueueEvent ev = events_.front();
        events_.pop();
        std::cout << "wait done, event popped" << std::endl;
        return ev;
    } else if (workCount_ < 1) {// Else if queue has no workers, stop
        std::cout << "wait done, was no more work, returning" << std::endl;
        return nullptr;
    }
    // This should not happen!
    std::cout << "THIS SHOULD NOT HAPPEN! Wait done queue empty, workCount: " << workCount_ << std::endl;
    return nullptr;
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
