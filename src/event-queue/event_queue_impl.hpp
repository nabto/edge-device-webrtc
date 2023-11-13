#pragma once

#include "event_queue.hpp"

#include <memory>
#include <functional>
#include <mutex>
#include <thread>
#include <future>
#include <queue>

namespace nabto {

class EventQueueImpl
    : public EventQueue
{
public:
    EventQueueImpl();
    ~EventQueueImpl();

    static EventQueuePtr create() {
        return std::make_shared<EventQueueImpl>();
    }

    void start();
    void post(QueueEvent event);
    void stop();

private:
    static void eventRunner(EventQueueImpl* self);
    QueueEvent pop();

    std::mutex mutex_;
    bool stopped_ = true;
    std::queue<QueueEvent> events_;
    std::thread eventThread_;
    std::condition_variable cond_;

};


} // namespace
