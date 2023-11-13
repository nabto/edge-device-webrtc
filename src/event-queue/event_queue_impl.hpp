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

    enum State {
        IDLE,
        RUNNING,
        STOPPED
    };

    EventQueueImpl();
    ~EventQueueImpl();

    static EventQueuePtr create() {
        return std::make_shared<EventQueueImpl>();
    }

    void run();
    void post(QueueEvent event);
    void stop();

private:
    static void eventRunner(EventQueueImpl* self);
    QueueEvent pop();

    std::mutex mutex_;
    bool running_ = false;
    enum State state_ = STOPPED;
    std::queue<QueueEvent> events_;
    std::thread eventThread_;
    std::condition_variable cond_;

};


} // namespace
