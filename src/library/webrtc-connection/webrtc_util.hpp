#pragma once

#include "webrtc_connection.hpp"

#include <rtc/rtc.hpp>

#include <sstream>

namespace nabto {

namespace webrtc_util {

bool parseTurnServers(rtc::Configuration& conf, std::vector<WebrtcConnection::TurnServer>& turnServers) {
    for (auto t : turnServers) {
        std::string proto = "";
        if (t.username.empty()) {
            proto = "stun:";
        } else {
            proto = "turn:";
        }

        for (auto u : t.urls) {
            std::stringstream ss;
            std::string host = u;
            if (host.rfind("turn:", 0) == 0 || host.rfind("stun:", 0) == 0) {
                host = host.substr(5);
            }
            ss << proto;
            if (!t.username.empty() && !t.credential.empty()) {
                std::string username = t.username;
                auto n = username.find(":");
                while (n != std::string::npos) {
                    username.replace(n, 1, "%3A");
                    n = username.find(":");
                }
                ss << username << ":" << t.credential << "@";
            }
            ss << host;

            auto server = rtc::IceServer(ss.str());
            NPLOGD << "Created server with hostname: " << server.hostname << std::endl
                << "    port: " << server.port << std::endl
                << "    username: " << server.username << std::endl
                << "    password: " << server.password << std::endl
                << "    type: " << (server.type == rtc::IceServer::Type::Turn ? "TURN" : "STUN") << std::endl
                << "    RelayType: " << (server.relayType == rtc::IceServer::RelayType::TurnUdp ? "TurnUdp" : (server.relayType == rtc::IceServer::RelayType::TurnTcp ? "TurnTcp" : "TurnTls"));
            conf.iceServers.push_back(server);
        }

    }

    return true;
}

} }// Namespaces
