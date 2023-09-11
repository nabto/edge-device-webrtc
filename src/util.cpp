#include "util.hpp"

#include <iostream>

namespace nabto {
namespace terminationWaiter {

std::promise<void> promise_;

void signal_handler(int s)
{
    (void)s;
    promise_.set_value();

}

void waitForTermination() {
    signal(SIGINT, &signal_handler);

    std::future<void> f = promise_.get_future();
    f.get();

}
} // namespace terminationWaiter



CurlAsyncPtr CurlAsync::create()
{
    auto c = std::make_shared<CurlAsync>();
    if (c->init()) {
        return c;
    }
    return nullptr;
}

CurlAsync::CurlAsync()
{
    std::cout << "CurlAsync constructor" << std::endl;

}

CurlAsync::~CurlAsync()
{
    std::cout << "CurlAsync destructor" << std::endl;
    try {
        thread_ = std::thread();
    } catch (std::exception& ex) {
        std::cout << "~CurlAsync exception " << ex.what() << std::endl;
    }
    curl_easy_cleanup(curl_);
    curl_global_cleanup();
}

bool CurlAsync::init()
{
    CURLcode res;
    res = curl_global_init(CURL_GLOBAL_ALL);
    if (res != CURLE_OK) {
        std::cout << "Failed to initialize Curl global" << std::endl;
        return false;
    }
    curl_ = curl_easy_init();
    if (!curl_) {
        std::cout << "Failed to initialize Curl easy" << std::endl;
        return false;
    }

    res = curl_easy_setopt(curl_, CURLOPT_VERBOSE, 0L);
    if (res != CURLE_OK) {
        std::cout << "Failed to set curl logging option" << std::endl;
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 1L);
    if (res != CURLE_OK) {
        std::cout << "Failed to set Curl progress option" << std::endl;
        return false;
    }
    return true;
}

void CurlAsync::stop()
{
    std::cout << "CurlAsync stopped" << std::endl;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    std::cout << "CurlAsync stop joined" << std::endl;
}

bool CurlAsync::asyncInvoke(std::function<void(CURLcode res)> callback)
{
    std::cout << "CurlAsync asyncInvoke" << std::endl;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!(thread_.get_id() == std::thread::id())) {
            // Thread is already running
            return false;
        }
    }
    // Thread is not running, no concurrency issues
    callback_ = callback;
    reinvoke_ = true;
    me_ = shared_from_this();
    thread_ = std::thread(threadRunner, this);
    return true;
}

void CurlAsync::asyncReinvoke(std::function<void(CURLcode res)> callback)
{
    std::cout << "CurlAsync asyncReinvoke" << std::endl;
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
    reinvoke_ = true;
}

void CurlAsync::threadRunner(CurlAsync* self)
{
    CURLcode res = CURLE_OK;
    while (true) {
        std::function<void(CURLcode res)> cb;
        {
            std::lock_guard<std::mutex> lock(self->mutex_);
            if (self->stopped_ || !self->reinvoke_) {
                break;
            }
            res = curl_easy_perform(self->curl_);
            self->reinvoke_ = false;
            cb = self->callback_;
            self->callback_ = nullptr;
        }
        cb(res);
    }
    CurlAsyncPtr me = nullptr;
    std::lock_guard<std::mutex> lock(self->mutex_);
    if (!self->stopped_) {
        self->thread_.detach();
    }
    me = self->me_;
    self->me_ = nullptr;
}

} // namespace
