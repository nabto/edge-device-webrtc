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
    void addWork();
    void removeWork();

private:
    void eventRunner();
    QueueEvent pop();

    std::mutex mutex_;
    std::queue<QueueEvent> events_;
    std::condition_variable cond_;

    int workCount_ = 0;

};


} // namespace
