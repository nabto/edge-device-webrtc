#pragma once

#include <curl/curl.h>

#include <signal.h>
#include <future>


namespace nabto {
namespace terminationWaiter {

void signal_handler(int s);

void waitForTermination();

} // namespace terminationWaiter

class CurlAsync;
typedef std::shared_ptr<CurlAsync> CurlAsyncPtr;

class CurlAsync : public std::enable_shared_from_this<CurlAsync> {
public:
    static CurlAsyncPtr create();
    CurlAsync();
    ~CurlAsync();

    bool init();
    void stop();

    // Get the curl reference to build your request on using `curl_easy_XX()` functions
    CURL* getCurl() { return curl_; }

    // Invokes the request. This starts a new thread in which the request is invoked synchroneusly. The callback is invoked from the created thread once the request is resolved.
    // Returns false if a thread is already running
    bool asyncInvoke(std::function<void (CURLcode res)> callback);

    // Reinvoke the request. This must be called from the callback of a previous request. This request will reuse the std::thread created for the first request. getCurl() can be used to build a new request in the first callback before calling this.
    void asyncReinvoke(std::function<void(CURLcode res)> callback);

    // Reinvoke the request. This must be called from the callback of a previous request and is a direct blocking invocation of the curl_easy_perform.
    CURLcode reinvoke();


private:
    static void threadRunner(CurlAsync*  self);

    CURL* curl_;
    std::thread thread_;
    std::mutex mutex_;
    std::function<void(CURLcode res)> callback_;
    bool reinvoke_ = false;
    bool stopped_ = false;
    CurlAsyncPtr me_ = nullptr;

};

} // namespace nabto
