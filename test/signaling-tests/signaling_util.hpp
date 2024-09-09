#include <event-queue/event_queue_impl.hpp>
#include <nabto/nabto_device_webrtc.hpp>
#include <nabto/nabto_device_virtual.h>

#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>

#include <rtc/global.hpp>
#include <nlohmann/json.hpp>
#include <iostream>

namespace nabto {
namespace test {

static std::vector<uint8_t> jsonToStreamBuffer(nlohmann::json in) {
    auto inStr = in.dump();
    auto inStrLen = inStr.size();
    std::vector<uint8_t> out;
    out.push_back(inStrLen & 0xFF);
    out.push_back((inStrLen & 0xFF00) >> 8);
    out.push_back((inStrLen & 0xFF0000) >> 16);
    out.push_back((inStrLen & 0xFF000000) >> 24);
    for (size_t i = 0; i < inStrLen; i++) {
        out.push_back(inStr[i]);
    }
    return out;
}

static nlohmann::json streamBufferToJson(uint8_t* buff, size_t len) {
    std::string str((char*)buff, len);
    return nlohmann::json::parse(str);
}

class VirtualCoap {
public:
    VirtualCoap(NabtoDevicePtr device, NabtoDeviceVirtualConnection* conn, NabtoDeviceCoapMethod method, const char* path) {
        req_ = nabto_device_virtual_coap_request_new(conn, method, path);
        future_ = nabto_device_future_new(device.get());
    }

    ~VirtualCoap() {
        nabto_device_virtual_coap_request_free(req_);
        nabto_device_future_free(future_);
        std::cout << "COAP destroyed" << std::endl;
    }

    void execute(std::function<void(NabtoDeviceError ec)> cb) {
        cb_ = cb;
        nabto_device_virtual_coap_request_execute(req_, future_);
        nabto_device_future_set_callback(future_, responseReady, this);
    }

    nlohmann::json getJsonResp() {
        uint16_t cf;
        BOOST_TEST(nabto_device_virtual_coap_request_get_response_content_format(req_, &cf) == NABTO_DEVICE_EC_OK);
        BOOST_TEST(cf == NABTO_DEVICE_COAP_CONTENT_FORMAT_APPLICATION_JSON);

        char* payload;
        size_t len;
        BOOST_TEST(nabto_device_virtual_coap_request_get_response_payload(req_, (void**)&payload, &len) == NABTO_DEVICE_EC_OK);

        std::string pl(payload, len);
        try {
            auto json = nlohmann::json::parse(pl);
            return json;
        } catch (std::exception& ex) {
            BOOST_TEST(ex.what() == "");
            return nlohmann::json();
        }
    }

    NabtoDeviceVirtualCoapRequest* getRequest() { return req_; }

private:
    static void responseReady(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
    {
        VirtualCoap* self = (VirtualCoap*)userData;
        self->cb_(ec);
        self->cb_ = nullptr;
    }

    NabtoDeviceVirtualCoapRequest* req_;
    NabtoDeviceFuture* future_ = NULL;
    std::function<void(NabtoDeviceError ec)> cb_;

};

class VirtualStream {
public:
    VirtualStream(NabtoDevicePtr device, NabtoDeviceVirtualConnection* conn) {
        stream_ = nabto_device_virtual_stream_new(conn);
        future_ = nabto_device_future_new(device.get());
        device_ = device;
    }

    ~VirtualStream() {
        if (readBuffer_) {
            free(readBuffer_);
        }
        nabto_device_virtual_stream_free(stream_);
        nabto_device_future_free(future_);
        std::cout << "Stream destroyed" << std::endl;
    }

    void open(uint32_t port, std::function<void(NabtoDeviceError ec)> cb) {
        openCb_ = cb;
        nabto_device_virtual_stream_open(stream_, future_, port);
        nabto_device_future_set_callback(future_, streamOpened, this);

    }

    void write(std::vector<uint8_t>& data, std::function<void(NabtoDeviceError ec)> cb) {
        std::cout << "stream write" << std::endl;
        writeCb_ = cb;
        auto fut = nabto_device_future_new(device_.get());
        nabto_device_virtual_stream_write(stream_, fut, data.data(), data.size());
        nabto_device_future_set_callback(fut, streamWritten, this);
    }

    void readAll(size_t len, std::function<void(NabtoDeviceError ec, uint8_t* buff, size_t len)> cb) {
        std::cout << "stream read all: " << len << std::endl;
        readCb_ = cb;
        if (readBuffer_) {
            free(readBuffer_);
        }
        readBuffer_ = (uint8_t*)calloc(len, 1);
        auto fut = nabto_device_future_new(device_.get());
        nabto_device_virtual_stream_read_all(stream_, fut, readBuffer_, len, &readLen_);
        nabto_device_future_set_callback(fut, streamRead, this);

    }

    void close(std::function<void(NabtoDeviceError ec)> cb) {
        std::cout << "stream close" << std::endl;
        closeCb_ = cb;
        nabto_device_virtual_stream_close(stream_, future_);
        nabto_device_future_set_callback(future_, streamClosed, this);
    }

    void abort() {
        std::cout << "stream abort" << std::endl;
        nabto_device_virtual_stream_abort(stream_);
    }

private:
    static void streamOpened(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
    {
        std::cout << "stream opened" << std::endl;
        VirtualStream* self = (VirtualStream*)userData;
        self->openCb_(ec);
        self->openCb_ = nullptr;
    }

    static void streamClosed(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
    {
        std::cout << "stream closed" << std::endl;
        VirtualStream* self = (VirtualStream*)userData;
        self->closeCb_(ec);
        self->closeCb_ = nullptr;
    }

    static void streamWritten(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
    {
        std::cout << "stream written" << std::endl;
        nabto_device_future_free(future);
        VirtualStream* self = (VirtualStream*)userData;
        auto cb = self->writeCb_;
        self->writeCb_ = nullptr;
        cb(ec);
    }

    static void streamRead(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
    {
        std::cout << "stream readen" << std::endl;
        nabto_device_future_free(future);
        VirtualStream* self = (VirtualStream*)userData;
        auto cb = self->readCb_;
        self->readCb_ = nullptr;
        cb(ec, self->readBuffer_, self->readLen_);
    }

    NabtoDeviceVirtualStream* stream_;
    NabtoDeviceFuture* future_ = NULL;
    NabtoDevicePtr device_;
    std::function<void(NabtoDeviceError ec)> openCb_;
    std::function<void(NabtoDeviceError ec)> closeCb_;
    std::function<void(NabtoDeviceError ec, uint8_t* buff, size_t len)> readCb_;
    std::function<void(NabtoDeviceError ec)> writeCb_;

    // uint8_t readBuffer_[1024];
    uint8_t* readBuffer_ = NULL;
    size_t readLen_;

};


class TestDevice : public std::enable_shared_from_this<TestDevice> {
public:
    TestDevice() :
        device_(nabto::makeNabtoDevice()),
        eventQueue_(nabto::EventQueueImpl::create())
    {
        NabtoDeviceError ec;
        char* logLevel = getenv("NABTO_LOG_LEVEL");
        if (logLevel != NULL) {
            ec = nabto_device_set_log_std_out_callback(NULL);
            ec = nabto_device_set_log_level(NULL, logLevel);
        }
        auto dev = device_.get();
        ec = nabto_device_set_server_url(dev, "server.foo.bar");
        char* key;
        nabto_device_create_private_key(dev, &key);
        ec = nabto_device_set_private_key(dev, key);
        BOOST_TEST(ec == NABTO_DEVICE_EC_OK);
        nabto_device_string_free(key);
        nabto_device_set_product_id(dev, "pr-test");
        nabto_device_set_device_id(dev, "de-test");
        nabto_device_set_local_port(dev, 0);
        nabto_device_set_p2p_port(dev, 0);
        nabto_device_enable_mdns(device_.get());

        char* fp;
        ec = nabto_device_get_device_fingerprint(device_.get(), &fp);
        BOOST_TEST(ec == NABTO_DEVICE_EC_OK);
        deviceFp_ = std::string(fp);
        nabto_device_string_free(fp);

        NabtoDeviceFuture* fut = nabto_device_future_new(device_.get());
        nabto_device_start(device_.get(), fut);

        ec = nabto_device_future_wait(fut);
        nabto_device_future_free(fut);
        BOOST_TEST(ec == NABTO_DEVICE_EC_OK);

        static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;

        char* envLvl = std::getenv("NABTO_WEBRTC_LOG_LEVEL");
        if (envLvl != NULL) {
            auto logLevel = std::string(envLvl);
            nabto::initLogger(plogSeverity(logLevel), &consoleAppender);
        }

        webrtc_ = nabto::NabtoDeviceWebrtc::create(eventQueue_, device_);

        webrtc_->setCheckAccessCallback([](NabtoDeviceConnectionRef ref, std::string action) -> bool {
            // TODO: custom test callback
            return true;
            });

        webrtc_->setTrackEventCallback([](NabtoDeviceConnectionRef connRef, nabto::MediaTrackPtr track) {
            // TODO: custom test callback
            });

    }

    ~TestDevice() {

        std::cout << "TestDevice destroyed" << std::endl;
        auto fut = rtc::Cleanup();
        fut.wait();
        fut.get();
    }

    nabto::NabtoDevicePtr getDevice() { return device_; }

    NabtoDeviceVirtualConnection* makeConnection()
    {
        auto connection_ = nabto_device_virtual_connection_new(device_.get());
        return connection_;
    }

    void makeConnectionSigStream(std::function<void(NabtoDeviceVirtualConnection* conn, std::shared_ptr<VirtualStream> stream)> cb) {
        conn_ = makeConnection();
        auto self = shared_from_this();
        coap_ = std::make_shared <VirtualCoap>(device_, conn_, NABTO_DEVICE_COAP_GET, "/p2p/webrtc-info");
        stream_ = std::make_shared<VirtualStream>(device_, conn_);

        coap_->execute([self, cb](NabtoDeviceError ec) {
            auto resp = self->coap_->getJsonResp();
            auto port = resp["SignalingStreamPort"].get<uint32_t>();
            self->stream_->open(port, [self, cb](NabtoDeviceError ec) {
                BOOST_TEST(ec == NABTO_DEVICE_EC_OK);
                cb(self->conn_, self->stream_);
                self->conn_ = NULL;
                // self->stream_ = nullptr;
                // self->coap_ = nullptr;
                });

            });
    }

    void makeConnectionSigV2Stream(std::function<void(NabtoDeviceVirtualConnection* conn, std::shared_ptr<VirtualStream> stream)> cb) {
        conn_ = makeConnection();
        auto self = shared_from_this();
        coap_ = std::make_shared <VirtualCoap>(device_, conn_, NABTO_DEVICE_COAP_GET, "/p2p/webrtc-info");
        stream_ = std::make_shared<VirtualStream>(device_, conn_);

        coap_->execute([self, cb](NabtoDeviceError ec) {
            auto resp = self->coap_->getJsonResp();
            auto port = resp["SignalingV2StreamPort"].get<uint32_t>();
            self->stream_->open(port, [self, cb](NabtoDeviceError ec) {
                BOOST_TEST(ec == NABTO_DEVICE_EC_OK);
                cb(self->conn_, self->stream_);
                self->conn_ = NULL;
                // self->stream_ = nullptr;
                // self->coap_ = nullptr;
                });

            });
    }

    void run() {
        eventQueue_->addWork();
        eventQueue_->run();
    }

    void close(std::function<void()> cb) {
        std::cout << "device close" << std::endl;
        closeCb_ = cb;
        NabtoDeviceFuture* fut = nabto_device_future_new(device_.get());
        nabto_device_close(device_.get(), fut);
        nabto_device_future_set_callback(fut, deviceClosed, this);
    }

    void stop() {
        std::cout << "device stop" << std::endl;
        auto self = shared_from_this();
        eventQueue_->post([self]() {
            std::cout << "device stop synced" << std::endl;
            self->stream_ = nullptr;
            self->coap_ = nullptr;
            nabto_device_stop(self->device_.get());
            self->webrtc_ = nullptr;
            if (self->closeCb_) {
                self->closeCb_();
            }
            });
        eventQueue_->removeWork();
    }

    static void deviceClosed(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData) {
        std::cout << "device closed" << std::endl;
        nabto_device_future_free(future);
        TestDevice* self = (TestDevice*)userData;
        self->stop();
    }

    enum plog::Severity plogSeverity(std::string& logLevel)
    {
        if (logLevel == "trace") {
            return plog::Severity::debug;
        }
        else if (logLevel == "warn") {
            return plog::Severity::warning;
        }
        else if (logLevel == "info") {
            return plog::Severity::info;
        }
        else if (logLevel == "error") {
            return plog::Severity::error;
        }
        return plog::Severity::none;

    }

    nabto::NabtoDevicePtr device_;
    nabto::EventQueueImplPtr eventQueue_;
    nabto::NabtoDeviceWebrtcPtr webrtc_;
    std::string deviceFp_;
    std::shared_ptr<VirtualStream> stream_ = nullptr;
    std::shared_ptr<VirtualCoap> coap_ = nullptr;
    NabtoDeviceVirtualConnection* conn_ = NULL;
    std::function<void()> closeCb_;

};


}
} // namespace

