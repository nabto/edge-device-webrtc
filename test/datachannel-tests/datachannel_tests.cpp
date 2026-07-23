#include <boost/test/unit_test.hpp>

#include "../test-common/virtual_device.hpp"

#include <rtc/rtc.hpp>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

namespace nabto {
namespace test {

// A minimal WebRTC client connecting to the device through the virtual
// signaling stream. It opens a datachannel and can then hang up like a real
// client app ending a call while the device application is still sending.
class TestWebrtcClient : public std::enable_shared_from_this<TestWebrtcClient> {
public:
    static std::shared_ptr<TestWebrtcClient> create(std::shared_ptr<VirtualStream> stream)
    {
        return std::make_shared<TestWebrtcClient>(stream);
    }

    TestWebrtcClient(std::shared_ptr<VirtualStream> stream) : stream_(stream) {}

    void start()
    {
        auto self = shared_from_this();
        rtc::Configuration conf;
        std::lock_guard<std::mutex> lock(mutex_);
        pc_ = std::make_shared<rtc::PeerConnection>(conf);

        pc_->onLocalDescription([self](rtc::Description desc) {
            nlohmann::json data = { {"sdp", std::string(desc)}, {"type", desc.typeString()} };
            self->sendSignaling({ {"type", 0 /*WEBRTC_OFFER*/}, {"data", data.dump()} });
        });

        pc_->onLocalCandidate([self](rtc::Candidate cand) {
            nlohmann::json data = { {"sdpMid", cand.mid()}, {"candidate", cand.candidate()} };
            self->sendSignaling({ {"type", 2 /*WEBRTC_ICE*/}, {"data", data.dump()} });
        });

        channel_ = pc_->createDataChannel("test");
        channel_->onOpen([self]() {
            std::lock_guard<std::mutex> lock(self->mutex_);
            self->channelOpen_ = true;
            self->cond_.notify_all();
        });
        // Discard incoming messages. Without a consumer the receive queue
        // fills up and blocks the SCTP processing thread, which would keep
        // the connection from shutting down.
        channel_->onMessage([](rtc::binary data) {}, [](std::string data) {});

        readMessage();
    }

    bool waitForChannelOpen(std::chrono::seconds timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cond_.wait_for(lock, timeout, [this]() { return channelOpen_; });
    }

    // The client hangs up: closing the peer connection makes the client SCTP
    // send an association SHUTDOWN. From the moment the device receives it,
    // usrsctp fails device-side sends with ECONNRESET until libdatachannel
    // has processed the state change and closed the datachannel. The
    // datachannel object is deliberately not closed first: closing it would
    // send a graceful stream reset marking the device channel closed before
    // the association dies, hiding the race.
    void hangup()
    {
        std::shared_ptr<rtc::PeerConnection> pc;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pc = pc_;
        }
        if (pc != nullptr) {
            pc->close();
        }
    }

    // Actual destruction for test cleanup after all sending has stopped.
    void destroy()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        channel_ = nullptr;
        pc_ = nullptr;
    }

private:
    void sendSignaling(const nlohmann::json& msg)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        writeQueue_.push_back(jsonToStreamBuffer(msg));
        if (!writing_) {
            writing_ = true;
            writeNext();
        }
    }

    // requires mutex_ locked
    void writeNext()
    {
        auto self = shared_from_this();
        writeBuffer_ = writeQueue_.front();
        writeQueue_.pop_front();
        stream_->write(writeBuffer_, [self](NabtoDeviceError ec) {
            std::lock_guard<std::mutex> lock(self->mutex_);
            if (ec != NABTO_DEVICE_EC_OK || self->writeQueue_.empty()) {
                self->writing_ = false;
                return;
            }
            self->writeNext();
        });
    }

    void readMessage()
    {
        auto self = shared_from_this();
        stream_->readAll(4, [self](NabtoDeviceError ec, uint8_t* buff, size_t len) {
            if (ec != NABTO_DEVICE_EC_OK) {
                return;
            }
            uint32_t l = *((uint32_t*)buff);
            self->stream_->readAll(l, [self](NabtoDeviceError ec, uint8_t* buff, size_t len) {
                if (ec != NABTO_DEVICE_EC_OK) {
                    return;
                }
                self->handleMessage(streamBufferToJson(buff, len));
                self->readMessage();
            });
        });
    }

    void handleMessage(const nlohmann::json& msg)
    {
        std::shared_ptr<rtc::PeerConnection> pc;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pc = pc_;
        }
        if (pc == nullptr) {
            return;
        }
        try {
            int type = msg.at("type").get<int>();
            if (type == 1 /*WEBRTC_ANSWER*/) {
                auto data = nlohmann::json::parse(msg.at("data").get<std::string>());
                rtc::Description desc(data.at("sdp").get<std::string>(), data.at("type").get<std::string>());
                pc->setRemoteDescription(desc);
            } else if (type == 2 /*WEBRTC_ICE*/) {
                auto data = nlohmann::json::parse(msg.at("data").get<std::string>());
                rtc::Candidate cand(data.at("candidate").get<std::string>(), data.at("sdpMid").get<std::string>());
                pc->addRemoteCandidate(cand);
            }
        } catch (std::exception& ex) {
            std::cout << "client failed to handle signaling message: " << ex.what() << std::endl;
        }
    }

    std::shared_ptr<VirtualStream> stream_;
    std::shared_ptr<rtc::PeerConnection> pc_;
    std::shared_ptr<rtc::DataChannel> channel_;

    std::mutex mutex_;
    std::condition_variable cond_;
    bool channelOpen_ = false;

    std::deque<std::vector<uint8_t> > writeQueue_;
    std::vector<uint8_t> writeBuffer_;
    bool writing_ = false;
};

}
} // namespace

BOOST_AUTO_TEST_SUITE(datachannel_api)

// Reproduces a crash seen on customer devices: the client ends the call (or
// disconnects) while the application is still sending datachannel messages.
// The client SCTP shuts the association down, making usrsctp fail device-side
// sends with ECONNRESET, so libdatachannel throws
// std::runtime_error("Sending failed, errno=...") until the device-side
// channel is marked closed. Datachannel::sendMessage() must not leak that
// exception to the application; if it does, an application calling
// sendMessage() without a try/catch dies with std::terminate.
BOOST_AUTO_TEST_CASE(send_during_abrupt_client_disconnect_does_not_throw, *boost::unit_test::timeout(180))
{
    auto td = std::make_shared<nabto::test::TestDevice>();

    std::mutex mutex;
    std::condition_variable cond;
    nabto::DatachannelPtr deviceChannel = nullptr;
    std::atomic<bool> deviceChannelClosed(false);

    td->webrtc_->setDatachannelEventCallback([&](NabtoDeviceConnectionRef connRef, nabto::DatachannelPtr channel) {
        channel->setCloseCallback([&deviceChannelClosed]() {
            deviceChannelClosed = true;
        });
        std::lock_guard<std::mutex> lock(mutex);
        deviceChannel = channel;
        cond.notify_all();
    });

    std::shared_ptr<nabto::test::TestWebrtcClient> client = nullptr;
    NabtoDeviceVirtualConnection* conn = NULL;

    td->makeConnectionSigStream([&](NabtoDeviceVirtualConnection* c, std::shared_ptr<nabto::test::VirtualStream> stream) {
        conn = c;
        client = nabto::test::TestWebrtcClient::create(stream);
        client->start();
    });

    std::string sendError;

    std::thread orchestrator([&]() {
        // Wait for the datachannel to be open on both peers
        bool deviceReady = false;
        {
            std::unique_lock<std::mutex> lock(mutex);
            deviceReady = cond.wait_for(lock, std::chrono::seconds(30), [&]() { return deviceChannel != nullptr; });
        }
        BOOST_TEST(deviceReady);
        bool clientReady = client->waitForChannelOpen(std::chrono::seconds(30));
        BOOST_TEST(clientReady);

        if (deviceReady && clientReady) {
            // Send messages from an application thread like an application
            // feeding data to a client
            std::atomic<bool> stopSending(false);
            std::thread sender([&]() {
                std::vector<uint8_t> buffer(256, 0x42);
                while (!stopSending && !deviceChannelClosed) {
                    try {
                        deviceChannel->sendMessage(buffer.data(), buffer.size());
                    } catch (std::exception& ex) {
                        std::lock_guard<std::mutex> lock(mutex);
                        sendError = ex.what();
                        return;
                    }
                }
            });

            // Let some data flow, then let the client hang up mid-stream
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            client->hangup();

            // Keep sending through the teardown window
            auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (std::chrono::steady_clock::now() < deadline && sendError.empty() && !deviceChannelClosed) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            stopSending = true;
            sender.join();
            client->destroy();
        }

        td->close([]() {
            });
    });

    td->run();
    orchestrator.join();

    BOOST_TEST(sendError == "", "Datachannel::sendMessage() threw an exception: " + sendError);

    if (conn != NULL) {
        nabto_device_virtual_connection_free(conn);
    }
}

BOOST_AUTO_TEST_SUITE_END()
