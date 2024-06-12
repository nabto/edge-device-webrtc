#include "fifo_file_client.hpp"

#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <chrono>

const int FIFO_BUFFER_SIZE = 4096;


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

    FifoTrack track = {
        media,
        packetizer_->createPacketizer(ssrc, pt)
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
            close(fd_);
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

    self->fd_ = open(self->filePath_.c_str(), O_RDONLY, O_NONBLOCK);

    NPLOGI << "file opened at: " << self->filePath_ << " fd: " << self->fd_;

    fd_set rfdset;
    int maxfd;
    int retval = 0;

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    FD_ZERO(&rfdset);
    FD_SET(self->fd_, &rfdset);


    while ((retval = select(self->fd_+1, &rfdset, NULL, NULL, &tv)) != -1) {
        tv.tv_sec = 5;

        {
            std::lock_guard<std::mutex> lock(self->mutex_);
            if (self->stopped_) {
                break;
            }
        }
        if (!retval) {
            NPLOGD << "Select timeout: " << retval;
            continue;
        }

        try {
            int r = read(self->fd_, buffer, FIFO_BUFFER_SIZE);
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

            std::vector<uint8_t> data(buffer, buffer+r);
            {
                std::lock_guard<std::mutex> lock(self->mutex_);
                for (const auto& [key, value] : self->mediaTracks_) {
                    auto packets = value.packetizer->incoming(data);
                    for (auto p : packets) {
                        value.track->send(p.data(), p.size());
                    }
                }
            }
        } catch (std::exception& ex) {
            NPLOGE << "Failed to read from FIFO: " << ex.what();
        }
    }
    NPLOGI << "File reader thread returning";


}


} // namespace
