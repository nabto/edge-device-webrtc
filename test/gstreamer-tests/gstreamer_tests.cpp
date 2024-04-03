#include <boost/test/unit_test.hpp>

#include <rtsp-client/rtsp_client.hpp>
#include <track-negotiators/h264.hpp>
#include <track-negotiators/opus.hpp>

#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>


#include <iostream>

namespace nabto {
namespace test {

const std::string pipeline = "'(  videotestsrc ! clockoverlay ! video/x-raw,width=640,height=480 ! videoconvert ! queue ! x264enc tune=\"zerolatency\" byte-stream=true bitrate=1000 key-int-max=30 ! video/x-h264, profile=constrained-baseline ! rtph264pay name=pay0 pt=96 )'";

class RtspTestServer : public std::enable_shared_from_this<RtspTestServer> {
public:
    enum AUTH_TYPE {
        NONE,
        BASIC,
        DIGEST
    };

    RtspTestServer() {
        gst_init(NULL, NULL);
    }

    RtspTestServer(std::string username, std::string password, enum AUTH_TYPE type) : authType_(type), username_(username), password_(password)
    {
        gst_init(NULL, NULL);
    }

    ~RtspTestServer() {
        // gst_deinit();

    }

    bool run()
    {
        loop_ = g_main_loop_new(NULL, FALSE);
        server_ = gst_rtsp_server_new();
        gst_rtsp_server_set_service(server_, "0");
        auto mounts = gst_rtsp_server_get_mount_points(server_);
        auto factory = gst_rtsp_media_factory_new();

        gst_rtsp_media_factory_set_launch(factory, pipeline.c_str());
        gst_rtsp_media_factory_set_shared(factory, TRUE);
        std::string mount = "/video";
        gst_rtsp_mount_points_add_factory(mounts, mount.c_str(), factory);

        if (authType_ == BASIC) {
            gchar* basic;
            /* allow user and admin to access this resource */
            gst_rtsp_media_factory_add_role(factory, "user",
                GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
                GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, TRUE, NULL);
            gst_rtsp_media_factory_add_role(factory, "anonymous",
                GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
                GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, FALSE, NULL);

            /* make a new authentication manager */
            auto auth = gst_rtsp_auth_new();
            // gst_rtsp_auth_set_supported_methods(auth, GST_RTSP_AUTH_BASIC);

            /* make default token, it has no permissions */
            auto token =
                gst_rtsp_token_new(GST_RTSP_TOKEN_MEDIA_FACTORY_ROLE, G_TYPE_STRING,
                    "anonymous", NULL);
            gst_rtsp_auth_set_default_token(auth, token);
            gst_rtsp_token_unref(token);

            /* make user token */
            token =
                gst_rtsp_token_new(GST_RTSP_TOKEN_MEDIA_FACTORY_ROLE, G_TYPE_STRING,
                    "user", NULL);

            basic = gst_rtsp_auth_make_basic(username_.c_str(), password_.c_str());
            gst_rtsp_auth_add_basic(auth, basic, token);
            g_free(basic);

            gst_rtsp_token_unref(token);

            /* set as the server authentication manager */
            gst_rtsp_server_set_auth(server_, auth);
            g_object_unref(auth);

        } else if (authType_ == DIGEST) {
            gst_rtsp_media_factory_add_role(factory, "user",
                GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
                GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, TRUE, NULL);
            gst_rtsp_media_factory_add_role(factory, "anonymous",
                GST_RTSP_PERM_MEDIA_FACTORY_ACCESS, G_TYPE_BOOLEAN, TRUE,
                GST_RTSP_PERM_MEDIA_FACTORY_CONSTRUCT, G_TYPE_BOOLEAN, FALSE, NULL);

            /* make a new authentication manager */
            auto auth = gst_rtsp_auth_new();
            gst_rtsp_auth_set_supported_methods(auth, GST_RTSP_AUTH_DIGEST);

            /* make default token, it has no permissions */
            auto token =
                gst_rtsp_token_new(GST_RTSP_TOKEN_MEDIA_FACTORY_ROLE, G_TYPE_STRING,
                    "anonymous", NULL);
            gst_rtsp_auth_set_default_token(auth, token);
            gst_rtsp_token_unref(token);

            /* make user token */
            token =
                gst_rtsp_token_new(GST_RTSP_TOKEN_MEDIA_FACTORY_ROLE, G_TYPE_STRING,
                    "user", NULL);

            gst_rtsp_auth_add_digest(auth, username_.c_str(), password_.c_str(), token);
            gst_rtsp_token_unref(token);

            /* set as the server authentication manager */
            gst_rtsp_server_set_auth(server_, auth);
            g_object_unref(auth);
        }
        g_object_unref(mounts);

        sourceId_ = gst_rtsp_server_attach(server_, NULL);

        auto service = gst_rtsp_server_get_service(server_);
        servicePort_ = atoi(service);
        g_free(service);

        thread_ = std::thread(threadRunner, this);
        return true;
    }

    void stop()
    {

        g_source_remove(sourceId_);
        g_main_loop_quit(loop_);

        g_object_unref(server_);

        if (thread_.joinable()) {
            thread_.join();
        }
        g_main_loop_unref(loop_);
    }

    int getPort() {
        return servicePort_;
    }

    void runRtspTest(std::string& url, bool shouldFail = false) {
        auto rtsp = nabto::RtspClient::create("footrack", url);
        auto rtpVideoNegotiator = nabto::H264Negotiator::create();
        auto rtpAudioNegotiator = nabto::OpusNegotiator::create();
        rtsp->setTrackNegotiators(rtpVideoNegotiator, rtpAudioNegotiator);

        std::promise<void> promise;


        rtsp->start([&promise, rtsp, shouldFail](std::optional<std::string> error) {
            BOOST_TEST(error.has_value() == shouldFail);
            auto ret = rtsp->close([&promise, rtsp]() {
                promise.set_value();
                });
            BOOST_TEST(ret);
            });

        std::future<void> f = promise.get_future();
        f.get();
        rtsp->stop();
    }

private:
    enum AUTH_TYPE authType_ = NONE;
    std::string username_;
    std::string password_;

    GMainLoop* loop_;
    GstRTSPServer* server_;
    guint sourceId_ = 0;
    gint servicePort_ = 0;

    std::thread thread_;
    std::mutex mutex_;

    static void threadRunner(RtspTestServer* self)
    {
        g_main_loop_run(self->loop_);
    }

};


} } // Namespaces


BOOST_AUTO_TEST_SUITE(gstreamer)

// #include <unistd.h>
// BOOST_AUTO_TEST_CASE(test_server_start_stop, *boost::unit_test::timeout(180))
// {
//     nabto::test::RtspTestServer server;
//     BOOST_TEST(server.run());
//     sleep(1);
//     server.stop();
// }

BOOST_AUTO_TEST_CASE(can_negotiate_rtsp, *boost::unit_test::timeout(180))
{
    nabto::test::RtspTestServer server;
    BOOST_TEST(server.run());

    std::string url = "rtsp://127.0.0.1:" + std::to_string(server.getPort()) + "/video";
    // std::string url = "rtsp://127.0.0.1:8554/video";
    server.runRtspTest(url);
    server.stop();
}

BOOST_AUTO_TEST_CASE(rtsp_client_no_close, *boost::unit_test::timeout(180))
{
    nabto::test::RtspTestServer server;
    BOOST_TEST(server.run());

    std::string url = "rtsp://127.0.0.1:" + std::to_string(server.getPort()) + "/video";
    // std::string url = "rtsp://127.0.0.1:8554/video";
    auto rtsp = nabto::RtspClient::create("footrack", url);
    auto rtpVideoNegotiator = nabto::H264Negotiator::create();
    auto rtpAudioNegotiator = nabto::OpusNegotiator::create();
    rtsp->setTrackNegotiators(rtpVideoNegotiator, rtpAudioNegotiator);

    std::promise<void> promise;

    rtsp->start([&promise, rtsp](std::optional<std::string> error) {
        BOOST_TEST(!error.has_value());
        promise.set_value();
    });

    std::future<void> f = promise.get_future();
    f.get();
    rtsp->stop();
    server.stop();
}

#ifdef NABTO_RTSP_HAS_DIGEST_AUTH
BOOST_AUTO_TEST_CASE(can_digest_auth, *boost::unit_test::timeout(180))
{
    nabto::test::RtspTestServer server("username", "password", nabto::test::RtspTestServer::DIGEST);
    BOOST_TEST(server.run());

    std::string url = "rtsp://username:password@127.0.0.1:" + std::to_string(server.getPort()) + "/video";
    // std::string url = "rtsp://127.0.0.1:8554/video";
    server.runRtspTest(url);
    server.stop();
}
#endif

#ifdef NABTO_RTSP_HAS_BASIC_AUTH
BOOST_AUTO_TEST_CASE(can_basic_auth, *boost::unit_test::timeout(180))
{
    nabto::test::RtspTestServer server("username", "password", nabto::test::RtspTestServer::BASIC);
    BOOST_TEST(server.run());

    std::string url = "rtsp://username:password@127.0.0.1:" + std::to_string(server.getPort()) + "/video";
    // std::string url = "rtsp://127.0.0.1:8554/video";
    server.runRtspTest(url);
    server.stop();
}
#endif

BOOST_AUTO_TEST_CASE(can_enforce_digest_auth, *boost::unit_test::timeout(180))
{
    nabto::test::RtspTestServer server("username", "password", nabto::test::RtspTestServer::DIGEST);
    BOOST_TEST(server.run());

    std::string url = "rtsp://usere:passd@127.0.0.1:" + std::to_string(server.getPort()) + "/video";
    // std::string url = "rtsp://127.0.0.1:8554/video";
    server.runRtspTest(url, true);
    server.stop();
}

BOOST_AUTO_TEST_CASE(can_enforce_basic_auth, *boost::unit_test::timeout(180))
{
    nabto::test::RtspTestServer server("username", "password", nabto::test::RtspTestServer::BASIC);
    BOOST_TEST(server.run());

    std::string url = "rtsp://useme:passd@127.0.0.1:" + std::to_string(server.getPort()) + "/video";
    // std::string url = "rtsp://127.0.0.1:8554/video";
    server.runRtspTest(url, true);
    server.stop();
}

BOOST_AUTO_TEST_SUITE_END()
