#pragma once

#include <rtc/rtc.hpp>

#include <sys/socket.h>
typedef int SOCKET;

#include <string>
#include <memory>
#include <thread>
#include <iostream>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>

const int RTP_BUFFER_SIZE = 2048;

namespace nabto {

class RtcpClient;
typedef std::shared_ptr<RtcpClient> RtcpClientPtr;

class RtcpClient : public std::enable_shared_from_this<RtcpClient>
{
public:
    enum PayloadType {
        SENDER_REPORT = 200,
        RECEIVER_REPORT,
    };

    static RtcpClientPtr create(uint16_t port)
    {
        return std::make_shared<RtcpClient>(port);
    }

    RtcpClient(uint16_t port)
        : port_(port)
    {

    }

    ~RtcpClient()
    {
    }

    void start()
    {
        std::cout << "Starting RTP Client listen on port " << port_ << std::endl;
        stopped_ = false;
        rtcpSock_ = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr("0.0.0.0");
        addr.sin_port = htons(port_);
        if (bind(rtcpSock_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::string err = "Failed to bind UDP socket on 0.0.0.0:";
            err += std::to_string(port_);
            std::cout << err << std::endl;
            throw std::runtime_error(err);
        }

        int rcvBufSize = 212992;
        setsockopt(rtcpSock_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvBufSize),
            sizeof(rcvBufSize));
        rtcpThread_ = std::thread(rtcpRunner, this);
    }

    void stop()
    {
        std::cout << "RtcpClient stopped" << std::endl;
        stopped_ = true;
        if (rtcpSock_ != 0) {
            close(rtcpSock_);
        }
        rtcpThread_.join();
        std::cout << "RtpClient thread joined" << std::endl;
    }

private:
    static void rtcpRunner(RtcpClient* self)
    {
        char buffer[RTP_BUFFER_SIZE];
        char writeBuffer[64];
        rtc::RtcpRr* rr = (rtc::RtcpRr*) writeBuffer;
        memset(rr, 0, 64);
        int len;
        int count = 0;
        struct sockaddr_in srcAddr;
        socklen_t srcAddrLen = sizeof(srcAddr);
        while ((len = recvfrom(self->rtcpSock_, buffer, RTP_BUFFER_SIZE, 0, (struct sockaddr*)&srcAddr, &srcAddrLen)) >= 0 && !self->stopped_) {
            count++;
            if (count % 100 == 0) {
                std::cout << ".";
            }
            if (count % 1600 == 0) {
                std::cout << std::endl;
                count = 0;
            }

            if (len < 28) {
                std::cout << "too small: " << len << "<" << 28 << std::endl;
                continue;
            }
            auto sr = reinterpret_cast<rtc::RtcpSr*>(buffer);
            rtc::RtcpReportBlock* rb = rr->getReportBlock(0);
            rb->preparePacket(sr->senderSSRC(), 0, 0, 0, 0, 0, sr->ntpTimestamp(), 0);
            rr->preparePacket(1, 1);

            auto ret = sendto(self->rtcpSock_, rr, rr->header.lengthInBytes(), 0, (struct sockaddr*)&srcAddr, srcAddrLen);
        }
    }

    bool stopped_ = true;
    uint16_t port_ = 0;
    uint16_t remotePort_ = 6002;
    std::string remoteHost_ = "127.0.0.1";
    SOCKET rtcpSock_ = 0;
    std::thread rtcpThread_;

};

} // namespace
