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

    RtspTestServer(std::string& username, std::string& password, enum AUTH_TYPE type) : authType_(type), username_(username), password_(password)
    {
        gst_init(NULL, NULL);
    }

    ~RtspTestServer() {
        gst_deinit();

    }

    bool run()
    {
        loop_ = g_main_loop_new(NULL, FALSE);
        server_ = gst_rtsp_server_new();
        gst_rtsp_server_set_service(server_, "54321");
        mounts = gst_rtsp_server_get_mount_points(server_);
        factory_ = gst_rtsp_media_factory_new();

        gst_rtsp_media_factory_set_launch(factory_, pipeline.c_str());
        gst_rtsp_media_factory_set_shared(factory_, TRUE);
        std::string mount = "/video";
        gst_rtsp_mount_points_add_factory(mounts, mount.c_str(), factory_);

        if (authType_ == BASIC) {

        } else if (authType_ == DIGEST) {

        }
        g_object_unref(mounts);

        sourceId_ = gst_rtsp_server_attach(server_, NULL);

        thread_ = std::thread(threadRunner, this);
        return true;
    }

    void stop()
    {

        g_source_remove(sourceId_);
        g_main_loop_quit(loop_);

        GstRTSPSessionPool* pool;

        // auto smounts = gst_rtsp_server_get_mount_points(server_);
        // if (nullptr != smounts){
        //     gst_rtsp_mount_points_remove_factory(smounts, "/video");
        // }
        // g_object_unref(smounts);
        g_print("Ref Count: %u\n", GST_OBJECT_REFCOUNT_VALUE(server_));

        g_object_unref(server_);
        // g_object_unref(factory_);

        if (thread_.joinable()) {
            thread_.join();
        }
        g_main_loop_unref(loop_);
    }

private:
    enum AUTH_TYPE authType_ = NONE;
    std::string username_;
    std::string password_;

    GstRTSPMountPoints* mounts;
    GMainLoop* loop_;
    GstRTSPServer* server_;
    GstRTSPMediaFactory* factory_;
    guint sourceId_ = 0;
    // GstRTSPAuth* auth_;
    // GstRTSPToken* token_;

    std::thread thread_;
    std::mutex mutex_;

    static void threadRunner(RtspTestServer* self)
    {
        g_main_loop_run(self->loop_);
        // g_source_remove(self->sourceId_);
        // g_object_unref(self->factory_);
        // g_object_unref(self->server_);
        // g_main_loop_unref(self->loop_);
    }

};


} } // Namespaces

#include <unistd.h>

BOOST_AUTO_TEST_SUITE(gstreamer)

BOOST_AUTO_TEST_CASE(test_server_start_stop, *boost::unit_test::timeout(180))
{
    nabto::test::RtspTestServer server;
    BOOST_TEST(server.run());
    sleep(2);
    server.stop();
}

BOOST_AUTO_TEST_CASE(basic, *boost::unit_test::timeout(180))
{
    nabto::test::RtspTestServer server;
    BOOST_TEST(server.run());

    auto rtsp = nabto::RtspClient::create("footrack", "rtsp://127.0.0.1:54321/video");
    auto rtpVideoNegotiator = nabto::H264Negotiator::create();
    auto rtpAudioNegotiator = nabto::OpusNegotiator::create();
    rtsp->setTrackNegotiators(rtpVideoNegotiator, rtpAudioNegotiator);

    std::promise<void> promise;


    rtsp->start([&promise](std::optional<std::string> error) {
        if (error.has_value()) {
            std::cout << "Returned error string: " << error.value() << std::endl;
            BOOST_TEST(false);
        } else {
            std::cout << "SUCCESS!" << std::endl;
            BOOST_TEST(true);
        }
        promise.set_value();
    });

    std::future<void> f = promise.get_future();
    f.get();
    rtsp->stop();
    server.stop();
}

BOOST_AUTO_TEST_SUITE_END()
