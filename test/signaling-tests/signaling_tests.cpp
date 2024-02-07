#include <boost/test/unit_test.hpp>

#include <event-queue/event_queue_impl.hpp>

#include <nabto/nabto_device_webrtc.hpp>
#include <nabto/nabto_device_virtual.h>

#include <nlohmann/json.hpp>
#include <iostream>

namespace nabto {
namespace test {

class TestDevice {
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

        webrtc_ = nullptr;
    }

    nabto::NabtoDevicePtr getDevice() { return device_; }

    NabtoDeviceVirtualConnection* makeConnection()
    {
        auto connection_ = nabto_device_virtual_connection_new(device_.get());
        return connection_;
    }

    void run() {
        std::cout << "adding work" << std::endl;
        eventQueue_->addWork();
        eventQueue_->run();
    }

    void close() {
        NabtoDeviceFuture* fut = nabto_device_future_new(device_.get());
        nabto_device_close(device_.get(), fut);
        nabto_device_future_set_callback(fut, deviceClosed, this);
    }

    void stop() {
        auto device = device_.get();

        eventQueue_->post([device]() {
            nabto_device_stop(device);
            });
        std::cout << "Removing work" << std::endl;
        eventQueue_->removeWork();
    }

private:

    static void deviceClosed(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData) {
        TestDevice* self = (TestDevice*)userData;
        self->stop();
    }

    nabto::NabtoDevicePtr device_;
    nabto::EventQueueImplPtr eventQueue_;
    nabto::NabtoDeviceWebrtcPtr webrtc_;
    std::string deviceFp_;

};


class VirtualCoap {
public:
    VirtualCoap(NabtoDevicePtr device, NabtoDeviceVirtualConnection* conn, NabtoDeviceCoapMethod method, const char* path) {
        req_ = nabto_device_virtual_coap_request_new(conn, method, path);
        future_ = nabto_device_future_new(device.get());
    }

    ~VirtualCoap() {
        nabto_device_virtual_coap_request_free(req_);
        nabto_device_future_free(future_);
    }

    void execute(std::function<void (NabtoDeviceError ec)> cb) {
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

    NabtoDeviceVirtualCoapRequest* getRequest() {return req_;}

private:
    static void responseReady(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
    {
        VirtualCoap* self = (VirtualCoap*)userData;
        self->cb_(ec);

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

    }

    ~VirtualStream() {
        std::cout << "Virtual stream descructor" << std::endl;
        nabto_device_virtual_stream_free(stream_);
        nabto_device_future_free(future_);

    }

    void open(uint32_t port, std::function<void(NabtoDeviceError ec)> cb) {
        std::cout << "Stream open" << std::endl;

        openCb_ = cb;
        nabto_device_virtual_stream_open(stream_, future_, port);
        nabto_device_future_set_callback(future_, streamOpened, this);

    }

    void close(std::function<void(NabtoDeviceError ec)> cb) {
        closeCb_ = cb;
        nabto_device_virtual_stream_close(stream_, future_);
        nabto_device_future_set_callback(future_, streamClosed, this);

    }

    void abort() {
        nabto_device_virtual_stream_abort(stream_);
    }

private:
    static void streamOpened(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
    {
        std::cout << "Stream opened callback 1" << std::endl;
        VirtualStream* self = (VirtualStream*)userData;
        self->openCb_(ec);
    }

    static void streamClosed(NabtoDeviceFuture* future, NabtoDeviceError ec, void* userData)
    {
        std::cout << "Stream closed callback" << std::endl;
        VirtualStream* self = (VirtualStream*)userData;
        self->closeCb_(ec);
    }

    NabtoDeviceVirtualStream* stream_;
    NabtoDeviceFuture* future_ = NULL;
    std::function<void(NabtoDeviceError ec)> openCb_;
    std::function<void(NabtoDeviceError ec)> closeCb_;
    std::function<void(NabtoDeviceError ec)> readCb_;
    std::function<void(NabtoDeviceError ec)> writeCb_;

};

}
} // namespace


BOOST_AUTO_TEST_SUITE(signaling_api)

BOOST_AUTO_TEST_CASE(get_stream_port, *boost::unit_test::timeout(180))
{
    auto td = nabto::test::TestDevice();
    auto conn = td.makeConnection();

    auto coap = nabto::test::VirtualCoap(td.getDevice(), conn, NABTO_DEVICE_COAP_GET, "/webrtc/info");

    coap.execute([&td, &coap](NabtoDeviceError ec) {
        BOOST_TEST(ec == NABTO_DEVICE_EC_OK);
        auto req = coap.getRequest();
        uint16_t status;
        BOOST_TEST(nabto_device_virtual_coap_request_get_response_status_code(req, &status) == NABTO_DEVICE_EC_OK);
        BOOST_TEST(status == 205);

        uint16_t cf;
        BOOST_TEST(nabto_device_virtual_coap_request_get_response_content_format(req, &cf) == NABTO_DEVICE_EC_OK);
        BOOST_TEST(cf == NABTO_DEVICE_COAP_CONTENT_FORMAT_APPLICATION_JSON);

        char* payload;
        size_t len;
        BOOST_TEST(nabto_device_virtual_coap_request_get_response_payload(req, (void**)&payload, &len) == NABTO_DEVICE_EC_OK);

        std::string pl(payload, len);
        try {
            auto json = nlohmann::json::parse(pl);
            auto port = json["SignalingStreamPort"].get<uint32_t>();
            BOOST_TEST(port >= 0x80000000); // ephemeral ports are above this
        } catch(std::exception& ex) {
            BOOST_TEST(ex.what() == "");
        }

        td.stop();
    });

    td.run();

    nabto_device_virtual_connection_free(conn);
}

BOOST_AUTO_TEST_CASE(open_signaling_stream, *boost::unit_test::timeout(180))
{
    const std::string clientFp = "1234567890123456789012345678901212345678901234567890123456789012";

    auto td = nabto::test::TestDevice();
    auto conn = td.makeConnection();
    nabto_device_virtual_connection_set_client_fingerprint(conn, clientFp.c_str());

    auto coap = nabto::test::VirtualCoap(td.getDevice(), conn, NABTO_DEVICE_COAP_GET, "/webrtc/info");
    auto stream = nabto::test::VirtualStream(td.getDevice(), conn);

    coap.execute([&td, &conn, &coap, &stream](NabtoDeviceError ec) {
        auto resp = coap.getJsonResp();
        auto port = resp["SignalingStreamPort"].get<uint32_t>();
        std::cout << "Got signaling port: " << port << " opening stream" << std::endl;
        stream.open(port, [&td, &stream](NabtoDeviceError ec){
            std::cout << "Stream opened callback 2" << std::endl;
            BOOST_TEST(ec == NABTO_DEVICE_EC_OK);
            stream.abort();
            td.close();
            std::cout << "device stopped" << std::endl;
        });

    });

    td.run();

    nabto_device_virtual_connection_free(conn);
}


BOOST_AUTO_TEST_SUITE_END()
