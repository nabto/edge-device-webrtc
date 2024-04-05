#pragma once

#include "webrtc_connection.hpp"

#include <rtc/rtc.hpp>

#include <sstream>

namespace nabto {

namespace webrtc_util {

bool parseTurnServers(rtc::Configuration& conf, std::vector<WebrtcConnection::TurnServer>& turnServers) {
    for (auto t : turnServers) {
        std::string proto = "";
        std::string host = t.hostname;
        std::string query = "";
        if (t.hostname.rfind("turn:", 0) == 0) {
            host = t.hostname.substr(5);
            proto = "turn:";
        }
        else if (t.hostname.rfind("stun:", 0) == 0) {
            host = t.hostname.substr(5);
            proto = "stun:";
        } else {
            NPLOGE << "unknown url scheme: " << t.hostname;
            return false;
        }
        auto n = host.find("?");
        if (n != std::string::npos) {
            query = host.substr(n);
            host = host.substr(0, n);
        }

        std::stringstream ss;
        ss << proto;

        if (!t.username.empty() && !t.password.empty()) {
            std::string username = t.username;
            auto n = username.find(":");
            while (n != std::string::npos) {
                username.replace(n, 1, "%3A");
                n = username.find(":");
            }
            ss << username << ":" << t.password << "@";
        }

        ss << host;

        if (t.port != 0) {
            ss << ":" << t.port;
        }
        ss << query;

        auto server = rtc::IceServer(ss.str());
        NPLOGD << "Created server with hostname: " << server.hostname << std::endl
            << "    port: " << server.port << std::endl
            << "    username: " << server.username << std::endl
            << "    password: " << server.password << std::endl
            << "    type: " << (server.type == rtc::IceServer::Type::Turn ? "TURN" : "STUN") << std::endl
            << "    RelayType: " << (server.relayType == rtc::IceServer::RelayType::TurnUdp ? "TurnUdp" : (server.relayType == rtc::IceServer::RelayType::TurnTcp ? "TurnTcp" : "TurnTls"));
        conf.iceServers.push_back(server);
    }

    auto server = rtc::IceServer("stun:stun.nabto.net");
    NPLOGD << "Created server with hostname: " << server.hostname << std::endl
        << "    port: " << server.port << std::endl
        << "    username: " << server.username << std::endl
        << "    password: " << server.password << std::endl
        << "    type: " << (server.type == rtc::IceServer::Type::Turn ? "TURN" : "STUN") << std::endl
        << "    RelayType: " << (server.relayType == rtc::IceServer::RelayType::TurnUdp ? "TurnUdp" : (server.relayType == rtc::IceServer::RelayType::TurnTcp ? "TurnTcp" : "TurnTls"));
    conf.iceServers.push_back(server);
    return true;
}

} }// Namespaces
