#include <boost/test/unit_test.hpp>

#include "signaling_util.hpp"

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
