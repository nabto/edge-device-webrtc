#include "util.hpp"

#include <iostream>

namespace nabto {

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

}

CurlAsync::~CurlAsync()
{
    try {
        thread_ = std::thread();
    } catch (std::exception& ex) {
        NPLOGE << "~CurlAsync exception " << ex.what();
    }
    curl_easy_cleanup(curl_);
    curl_global_cleanup();
}

bool CurlAsync::init()
{
    CURLcode res;
    res = curl_global_init(CURL_GLOBAL_ALL);
    if (res != CURLE_OK) {
        NPLOGE << "Failed to initialize Curl global with: " << curl_easy_strerror(res);
        return false;
    }
    curl_ = curl_easy_init();
    if (!curl_) {
        NPLOGE << "Failed to initialize Curl easy with: " << curl_easy_strerror(res);
        return false;
    }

    res = curl_easy_setopt(curl_, CURLOPT_VERBOSE, 0L);
    if (res != CURLE_OK) {
        NPLOGE << "Failed to set curl logging option with: " << curl_easy_strerror(res);
        return false;
    }
    res = curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 1L);
    if (res != CURLE_OK) {
        NPLOGE << "Failed to set Curl progress option with: " << curl_easy_strerror(res);
        return false;
    }
    return true;
}

void CurlAsync::stop()
{
    NPLOGD << "CurlAsync stopped";
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    NPLOGD << "CurlAsync stop joined";
}

bool CurlAsync::asyncInvoke(std::function<void(CURLcode res, uint16_t statusCode)> callback)
{
    NPLOGD << "CurlAsync asyncInvoke";
    bool shouldReinvoke = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!(thread_.get_id() == std::thread::id())) {
            // Thread is already running
            shouldReinvoke = true;
        }
    }
    if (shouldReinvoke) {
        asyncReinvoke(callback);
        return true;
    }
    // Thread is not running, no concurrency issues
    callback_ = callback;
    reinvoke_ = true;
    me_ = shared_from_this();
    thread_ = std::thread(threadRunner, this);
    return true;
}

void CurlAsync::asyncReinvoke(std::function<void(CURLcode res, uint16_t statusCode)> callback)
{
    NPLOGD << "CurlAsync asyncReinvoke";
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
    reinvoke_ = true;
}

CURLcode CurlAsync::reinvoke()
{
    return curl_easy_perform(curl_);
}

void CurlAsync::reinvokeStatus(CURLcode* code, uint16_t* status)
{
    CURLcode res = curl_easy_perform(curl_);
    *code = res;
    if (res == CURLE_OK) {
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, status);
    }
}


void CurlAsync::threadRunner(CurlAsync* self)
{
    CURLcode res = CURLE_OK;
    while (true) {
        std::function<void(CURLcode res, uint16_t statusCode)> cb;
        long statusCode = 0;
        {
            std::lock_guard<std::mutex> lock(self->mutex_);
            if (self->stopped_ || !self->reinvoke_) {
                break;
            }
            res = curl_easy_perform(self->curl_);
            if (res == CURLE_OK) {
                curl_easy_getinfo(self->curl_, CURLINFO_RESPONSE_CODE, &statusCode);
            }
            self->reinvoke_ = false;
            cb = self->callback_;
            self->callback_ = nullptr;
        }
        cb(res, statusCode);
    }
    CurlAsyncPtr me = nullptr;
    std::lock_guard<std::mutex> lock(self->mutex_);
    if (!self->stopped_) {
        self->thread_.detach();
    }
    me = self->me_;
    self->me_ = nullptr;
}


bool fromHex(std::string& hex, uint8_t* data)
{
    // hexLength should be 2*datalength or (2*dataLength - 1)
    size_t dataLength = hex.size() / 2;
    memset(data, 0, dataLength);

    size_t index = 0;

    const char* end = hex.data() + hex.size();
    const char* ptr = hex.data();

    while (ptr < end) {
        char c = *ptr;
        uint8_t value = 0;
        if (c >= '0' && c <= '9')
            value = (c - '0');
        else if (c >= 'A' && c <= 'F')
            value = (10 + (c - 'A'));
        else if (c >= 'a' && c <= 'f')
            value = (10 + (c - 'a'));
        else {
            return false;
        }

        // shift each even hex byte 4 up
        data[(index / 2)] += value << (((index + 1) % 2) * 4);

        index++;
        ptr++;
    }

    return true;
}

std::string urlDecode(std::string in) {
    std::string out = "";
    for (auto i = in.begin(); i != in.end(); ++i) {
        if (*i == '%') {
            if (i + 1 == in.end() || i + 2 == in.end()) {
                NPLOGE << "Failed to URL decode: " << in << " string too short";
                return "";
            }
            std::string hex(i + 1, i + 3);
            char val;
            if (fromHex(hex, (uint8_t*)&val)) {
                out = out + val;
            }
            i = i + 2;
        }
        else {
            out = out + (*i);
        }
    }
    return out;
}


} // namespace
