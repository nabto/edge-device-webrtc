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
    bool doRun = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) {
            stopped_ = false;
            doRun = true;
        }
    }
    if (doRun) {
        eventRunner();
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
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        cond_.notify_one();
    }
}

void EventQueueImpl::addWork()
{
    workCount_++;
    std::cout << "work added: " << workCount_ << std::endl;
}

void EventQueueImpl::removeWork()
{
    workCount_--;
    std::cout << "work removed: " << workCount_ << std::endl;
}

QueueEvent EventQueueImpl::pop()
{
    std::unique_lock<std::mutex> lock(mutex_);
    // If no events AND (outstanding workers OR not stopped), wait for events
    if (events_.empty() && (workCount_ > 0 || !stopped_)) {
        std::cout << "no events, waiting. Work: " << workCount_ << std::endl;
        cond_.wait(lock);
    }
    // If we have an event, store it to be executed outside the lock
    if (!events_.empty()) {
        QueueEvent ev = events_.front();
        events_.pop();
        std::cout << "wait done, event popped" << std::endl;
        return ev;
    } else if (stopped_ && workCount_ < 1) {// Else if queue was stopped and no workers, stop
        std::cout << "wait done, was stopped, returning" << std::endl;
        return nullptr;
    }
    // This should not happen!
    std::cout << "Queue event was popped but queue was empty" << std::endl;
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
