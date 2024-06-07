#include "fifo_file_client.hpp"

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <chrono>

const int FIFO_BUFFER_SIZE = 204800;


namespace nabto {

FifoFileClientPtr FifoFileClient::create(const std::string& trackId, const std::string& filePath)
{
    return std::make_shared<FifoFileClient>(trackId, filePath);
}

FifoFileClient::FifoFileClient(const std::string& trackId, const std::string& filePath)
    : trackId_(trackId), filePath_(filePath)
{

}

FifoFileClient::~FifoFileClient()
{
}

bool FifoFileClient::isTrack(const std::string& trackId)
{
    if (trackId == trackId_) {
        return true;
    }
    return false;
}

bool FifoFileClient::matchMedia(MediaTrackPtr media)
{
    int pt = negotiator_->match(media);
    if (pt == 0) {
        NPLOGE << "    CODEC MATCHING FAILED!!! ";
        return false;
    }
    return true;
}

void FifoFileClient::addConnection(NabtoDeviceConnectionRef ref, MediaTrackPtr media)
{
    auto sdp = media->getSdp();

    rtc::Description::Media desc(sdp);
    auto pts = desc.payloadTypes();

    // exactly 1 payload type should exist, else something has failed previously, so we just pick the first one blindly.
    int pt = pts.empty() ? 0 : pts[0];

    const rtc::SSRC ssrc = negotiator_->ssrc();

    // TODO: use repacketizer interface so it becomes codec agnostic
    auto rtpConf = std::make_shared<rtc::RtpPacketizationConfig>(ssrc, media->getTrackId(), pt, 90000);


    FifoTrack track = {
        media,
        rtpConf,
        std::make_shared<rtc::H264RtpPacketizer>(rtc::NalUnit::Separator::StartSequence, rtpConf)
    };

    doAddConnection(ref, track);
}

void FifoFileClient::removeConnection(NabtoDeviceConnectionRef ref)
{
    NPLOGD << "Removing Nabto Connection from fifo";
    size_t mediaTracksSize = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);

        try {
            mediaTracks_.erase(ref);
        } catch (std::out_of_range& ex) {
            NPLOGE << "Tried to remove non-existing connection";
        }
        mediaTracksSize = mediaTracks_.size();
    }
    if (mediaTracksSize == 0) {
        NPLOGD << "Connection was last one. Stopping";
        stop();
    }
    else {
        NPLOGD << "Still " << mediaTracksSize << " Connections. Not stopping";
    }
}


void FifoFileClient::start()
{
    NPLOGI << "Starting fifo Client listen on file " << filePath_;
    fifo_ = std::ifstream(filePath_);
    if (!fifo_.good()) {
        NPLOGE << "Failed to open FIFO file at " << filePath_;
        return;
    }
    stopped_ = false;
    thread_ = std::thread(fifoRunner, this);
}

void FifoFileClient::stop()
{

    NPLOGD << "Fifo Client stopped";
    bool stopped = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopped_) {
            stopped = stopped_;
        }
        else {
            stopped_ = true;
            fifo_.close();
        }
    }
    if (!stopped && thread_.joinable()) {
        thread_.join();
    }
    mediaTracks_.clear();
    NPLOGD << "FIFO Client thread joined";
}

void FifoFileClient::doAddConnection(NabtoDeviceConnectionRef ref, FifoTrack track)
{

    std::lock_guard<std::mutex> lock(mutex_);
    mediaTracks_[ref] = track;
    NPLOGD << "Adding fifo connection";
    if (stopped_) {
        start();
    }
    // TODO: should file descriptors support downstream data? If so, setup track receive callback here.

}

void FifoFileClient::fifoRunner(FifoFileClient* self)
{
    char buffer[FIFO_BUFFER_SIZE];
    char* ptr = buffer;
    int count = 0;

    auto videoRtpSock_ = socket(AF_INET, SOCK_DGRAM, 0);

    // auto file = std::ofstream(self->filePath_+".test");


    while (self->fifo_.good()) {
        {
            std::lock_guard<std::mutex> lock(self->mutex_);
            if (self->stopped_) {
                break;
            }
        }

        try {
            // auto r = self->fifo_.readsome(buffer, FIFO_BUFFER_SIZE);
            self->fifo_.read(buffer, 20480);
            size_t r = 20480;
            if (r == 0) {
                continue;
            }
            count++;
            if (count % 100 == 0) {
                std::cout << ".";
            }
            if (count % 1600 == 0) {
                std::cout << std::endl;
                count = 0;
            }

            // file.write(ptr, r);

            // NPLOGE << "Read " << r << "bytes from fifo. " << "ptr+=r: " << ptr - buffer;

            std::vector<uint8_t> data(buffer, buffer+r);
            auto packets = self->packetizer_->incoming(data);
            {
                std::lock_guard<std::mutex> lock(self->mutex_);
                for (const auto& [key, value] : self->mediaTracks_) {
                    for (auto p : packets) {
                        value.track->send(p.data(), p.size());


                        struct sockaddr_in addr = {};
                        addr.sin_family = AF_INET;
                        addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                        addr.sin_port = htons(6001);

                        auto ret = sendto(videoRtpSock_, p.data(), p.size(), 0, (struct sockaddr*)&addr, sizeof(addr));
                    }
                }
            }
        } catch (std::exception& ex) {
            NPLOGE << "Failed to read from FIFO: " << ex.what();
        }
    }
    // file.write(buffer, ptr-buffer);

    NPLOGE << "File reader thread returning";


}


} // namespace
