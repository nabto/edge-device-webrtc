#include <boost/test/unit_test.hpp>

#include <event-queue/event_queue_impl.hpp>

#include <nabto/nabto_device_webrtc.hpp>
#include <nabto/nabto_device_virtual.h>

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


BOOST_AUTO_TEST_SUITE(signaling_api)

BOOST_AUTO_TEST_CASE(get_stream_port, *boost::unit_test::timeout(180))
{
    auto td = std::make_shared<nabto::test::TestDevice>();
    auto conn = td->makeConnection();

    auto coap = nabto::test::VirtualCoap(td->getDevice(), conn, NABTO_DEVICE_COAP_GET, "/p2p/webrtc-info");

    coap.execute([td, &coap](NabtoDeviceError ec) {
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

        td->close([]() {
            });
    });

    td->run();

    nabto_device_virtual_connection_free(conn);
}

BOOST_AUTO_TEST_CASE(open_signaling_stream, *boost::unit_test::timeout(180))
{
    auto td = std::make_shared<nabto::test::TestDevice>();
    auto conn = td->makeConnection();

    auto coap = nabto::test::VirtualCoap(td->getDevice(), conn, NABTO_DEVICE_COAP_GET, "/p2p/webrtc-info");
    auto stream = nabto::test::VirtualStream(td->getDevice(), conn);

    coap.execute([td, conn, &coap, &stream](NabtoDeviceError ec) {
        auto resp = coap.getJsonResp();
        auto port = resp["SignalingStreamPort"].get<uint32_t>();
        stream.open(port, [td, &stream, conn](NabtoDeviceError ec) {
            BOOST_TEST(ec == NABTO_DEVICE_EC_OK);
            stream.abort();
            td->close([conn]() {
                nabto_device_virtual_connection_free(conn);
                });
            });

        });

    td->run();

}

BOOST_AUTO_TEST_CASE(open_signaling_stream_helper, *boost::unit_test::timeout(180))
{
    auto td = std::make_shared<nabto::test::TestDevice>();
    td->makeConnectionSigStream([td](NabtoDeviceVirtualConnection* conn, std::shared_ptr<nabto::test::VirtualStream> stream) {
        stream->abort();
        td->close([conn]() {
            nabto_device_virtual_connection_free(conn);
            });

        });

    td->run();

}

BOOST_AUTO_TEST_CASE(get_turn_servers, *boost::unit_test::timeout(180))
{
    auto td = std::make_shared<nabto::test::TestDevice>();
    std::vector<uint8_t> req;
    td->makeConnectionSigStream([td, &req](NabtoDeviceVirtualConnection* conn, std::shared_ptr<nabto::test::VirtualStream> stream) {
        nlohmann::json turnReqjson = {{"type", 3}};
        req = nabto::test::jsonToStreamBuffer(turnReqjson);

        stream->write(req, [stream, td, conn](NabtoDeviceError ec) {
            BOOST_TEST(ec == NABTO_DEVICE_EC_OK);
            stream->readAll(4, [stream, td, conn](NabtoDeviceError ec, uint8_t* buff, size_t len) {
                BOOST_TEST(ec == NABTO_DEVICE_EC_OK);
                uint32_t l = *((uint32_t*)buff);
                stream->readAll(l, [stream, td, conn, l](NabtoDeviceError ec, uint8_t* buff, size_t len) {
                    BOOST_TEST(ec == NABTO_DEVICE_EC_OK);
                    auto resp = nabto::test::streamBufferToJson(buff, len);

                    auto type = resp["type"].get<uint32_t>();
                    BOOST_TEST(type == 4);

                    auto servs = resp["servers"].get<std::vector<nlohmann::json> >();
                    BOOST_TEST(servs.size() == 0);

                    auto iceservs = resp["iceServers"].get<std::vector<nlohmann::json> >();
                    BOOST_TEST(iceservs.size() == 0);

                    stream->abort();
                    td->close([conn]() {
                        nabto_device_virtual_connection_free(conn);
                        });

                    });
                });
            });
        });

    td->run();

}

BOOST_AUTO_TEST_CASE(answer_an_offer, *boost::unit_test::timeout(180))
{
    std::string offerData = "{\"sdp\":\"v=0\\r\\no=- 4001653510419693843 2 IN IP4 127.0.0.1\\r\\ns=-\\r\\nt=0 0\\r\\na=group:BUNDLE 0\\r\\na=extmap-allow-mixed\\r\\na=msid-semantic: WMS\\r\\nm=application 9 UDP/DTLS/SCTP webrtc-datachannel\\r\\nc=IN IP4 0.0.0.0\\r\\na=ice-ufrag:9aLM\\r\\na=ice-pwd:jtaHrFFgBekhsoOD+0pS3PaI\\r\\na=ice-options:trickle\\r\\na=fingerprint:sha-256 28:E0:2D:E0:11:02:A0:1A:39:8C:86:B2:19:11:5D:98:F3:8C:79:8F:56:08:52:E2:30:25:35:C9:67:FE:93:B7\\r\\na=setup:actpass\\r\\na=mid:0\\r\\na=sctp-port:5000\\r\\na=max-message-size:262144\\r\\n\",\"type\":\"offer\"}";
    auto td = std::make_shared<nabto::test::TestDevice>();
    std::vector<uint8_t> req;

    td->makeConnectionSigStream([td, &req, &offerData](NabtoDeviceVirtualConnection* conn, std::shared_ptr<nabto::test::VirtualStream> stream) {
        nlohmann::json offerReqjson = { {"type", 0}, {"data", offerData} };
        req = nabto::test::jsonToStreamBuffer(offerReqjson);

        stream->write(req, [stream, td, conn](NabtoDeviceError ec) {
            BOOST_TEST(ec == NABTO_DEVICE_EC_OK);
            stream->readAll(4, [stream, td, conn](NabtoDeviceError ec, uint8_t* buff, size_t len) {
                BOOST_TEST(ec == NABTO_DEVICE_EC_OK);
                uint32_t l = *((uint32_t*)buff);
                std::cout << "Reading " << l << " bytes" << std::endl;
                stream->readAll(l, [stream, td, conn, l](NabtoDeviceError ec, uint8_t* buff, size_t len) {
                    BOOST_TEST(ec == NABTO_DEVICE_EC_OK);
                    try {
                        auto resp = nabto::test::streamBufferToJson(buff, len);

                        auto type = resp["type"].get<uint32_t>();
                        BOOST_TEST(type == 1);

                        auto answerData = resp["data"].get<std::string>();
                        BOOST_TEST(answerData.size() > 0);
                        auto answerJson = nlohmann::json::parse(answerData);

                        auto dataType = answerJson["type"].get<std::string>();
                        BOOST_TEST(dataType == "answer");
                    } catch (std::exception& ex) {
                        std::cout << ex.what() << std::endl;
                        BOOST_TEST(ex.what() == "");
                    }
                    stream->close([stream, td, conn](NabtoDeviceError ec) {
                        stream->abort();
                        td->close([conn](){
                            nabto_device_virtual_connection_free(conn);
                        });
                        });
                    });
                });
            });
        });

    td->run();

}


BOOST_AUTO_TEST_SUITE_END()
