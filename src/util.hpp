#pragma once

#include <signal.h>
#include <future>


namespace nabto {
namespace terminationWaiter {

std::promise<void> promise_;


static void signal_handler(int s)
{
    (void)s;
    promise_.set_value();

}


static void waitForTermination() {
    signal(SIGINT, &signal_handler);

    std::future<void> f = promise_.get_future();
    f.get();

}

} } // namespaces
