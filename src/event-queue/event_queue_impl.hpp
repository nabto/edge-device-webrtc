#pragma once

#include "event_queue.hpp"

#include <memory>
#include <functional>
#include <mutex>
#include <thread>
#include <future>
#include <queue>

namespace nabto {

class EventQueueImpl;
typedef std::shared_ptr<EventQueueImpl> EventQueueImplPtr;


class EventQueueImpl
    : public EventQueue
{
public:
    EventQueueImpl();
    ~EventQueueImpl();

    static EventQueueImplPtr create() {
        return std::make_shared<EventQueueImpl>();
    }

    void run();
    void post(QueueEvent event);
    void stop();

private:
    void eventRunner();
    QueueEvent pop();

    std::mutex mutex_;
    bool stopped_ = true;
    std::queue<QueueEvent> events_;
    std::condition_variable cond_;

};


} // namespace
