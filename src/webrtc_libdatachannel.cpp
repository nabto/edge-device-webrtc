#include "webrtc_libdatachannel.hpp"
#include "webrtc_stream.hpp"

#include <nlohmann/json.hpp>
#include <sstream>

using nlohmann::json;

namespace nabto {


void WebrtcChannel::coapCallback(NabtoDeviceFuture* fut, NabtoDeviceError err, void* data)
{
    WebrtcChannel* self = (WebrtcChannel*)(data);
    nabto_device_future_free(fut);
    self->handleCoapResponse(err);
}

void WebrtcChannel::handleCoapResponse(NabtoDeviceError err)
{
    NabtoDeviceVirtualCoapRequest* req = coap_;
    coap_ = NULL;
    uint16_t status;
    nabto_device_virtual_coap_request_get_response_status_code(req, &status);
    uint16_t cf;
    nabto_device_virtual_coap_request_get_response_content_format(req, &cf);

    uint8_t* payload;
    size_t len;
    nabto_device_virtual_coap_request_get_response_payload(req, (void**)&payload, &len);
    std::vector<uint8_t> respPayload(payload, payload+len);

    json resp = {
        {"type", 1},
        {"requestId", coapRequestId_},
        {"statusCode", status},
        {"contentType", cf},
        {"payload", respPayload}
    };
    coapChannel_->send(resp.dump());
    nabto_device_virtual_coap_request_free(req);
}



void WebrtcChannel::createPeerConnection()
{
    auto self = shared_from_this();
    rtc::Configuration conf;

    for (auto t : turnServers_) {
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
            while(n != std::string::npos) {
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
        std::cout << "Created server with hostname: " << server.hostname << std::endl
        << "    port: " << server.port << std::endl
        << "    username: " << server.username << std::endl
        << "    password: " << server.password << std::endl
        << "    type: " << (server.type == rtc::IceServer::Type::Turn ? "TURN" : "STUN") << std::endl
        << "    RelayType: " << (server.relayType == rtc::IceServer::RelayType::TurnUdp ? "TurnUdp" : (server.relayType == rtc::IceServer::RelayType::TurnTcp ? "TurnTcp" : "TurnTls")) << std::endl;
        conf.iceServers.push_back(server);
    }

    // conf.iceTransportPolicy = rtc::TransportPolicy::Relay;

    conf.disableAutoNegotiation = true;

    pc_ = std::make_shared<rtc::PeerConnection>(conf);

    pc_->onStateChange([self](rtc::PeerConnection::State state) {
        std::cout << "State: " << state << std::endl;
        if (state == rtc::PeerConnection::State::Connected) {
            if (self->eventHandler_) {
                self->eventHandler_(ConnectionEvent::CONNECTED);
            }
        } else if (state == rtc::PeerConnection::State::Closed) {
            nabto_device_virtual_connection_free(self->nabtoConnection_);
            self->nabtoConnection_ = NULL;
            if (self->eventHandler_) {
                self->eventHandler_(ConnectionEvent::CLOSED);
            }
            self->eventHandler_ = nullptr;
            self->sendSignal_ = nullptr;
        }
        });

    pc_->onSignalingStateChange(
        [self](rtc::PeerConnection::SignalingState state) {
            std::cout << "Signalling State: " << state << std::endl;
            if (state ==
                rtc::PeerConnection::SignalingState::HaveLocalOffer) {
                auto description = self->pc_->localDescription();
                json message = {
                    {"type", description->typeString()},
                    {"sdp", std::string(description.value())} };
                auto type =
                    description->type() ==
                    rtc::Description::Type::Answer
                    ? WebrtcStream::ObjectType::WEBRTC_ANSWER
                    : WebrtcStream::ObjectType::WEBRTC_OFFER;
                json offer = {
                    {"type", type},
                    {"data", message}
                };
                std::cout << "SENDING " << type << " message: " << message << std::endl;
                auto data = offer.dump();
                self->sendSignal_(data);
            }
            if (state ==
                rtc::PeerConnection::SignalingState::HaveRemoteOffer) {
                try {
                    self->pc_->setLocalDescription();//(rtc::Description::Type::Answer);
                }
                catch (std::logic_error ex) {
                    std::cout << "EXCEPTION!!!! " << ex.what() << std::endl;
                }

                auto description = self->pc_->localDescription();
                std::cout << "local desc is type: " << description->typeString() << std::endl;
                json message = {
                    {"type", description->typeString()},
                    {"sdp", std::string(description.value())} };
                auto type =
                    description->type() ==
                    rtc::Description::Type::Answer
                    ? WebrtcStream::ObjectType::WEBRTC_ANSWER
                    : WebrtcStream::ObjectType::WEBRTC_OFFER;
                json offer = {
                    {"type", type},
                    {"data", message}
                };
                std::cout << "SENDING " << type << " message: " << message << std::endl;
                auto data = offer.dump();
                self->sendSignal_(data);
            }
        });

    pc_->onGatheringStateChange(
        [self](rtc::PeerConnection::GatheringState state) {
            std::cout << "Gathering State: " << state << std::endl;
            self->state_ = state;
        });

    pc_->onLocalCandidate([self](rtc::Candidate cand) {
        std::cout << "Got local candidate: " << cand << std::endl;
        json candidate;
        candidate["sdpMid"] = cand.mid();
        candidate["candidate"] = cand.candidate();
        json answer = { {"type", WebrtcStream::ObjectType::WEBRTC_ICE},
                        {"data", candidate} };
        if (true || self->state_ ==
            rtc::PeerConnection::GatheringState::Complete) {
            std::cout << "SENDING NEW-ICE-CANDIDATE" << std::endl;
            auto data = answer.dump();
            self->sendSignal_(data);
        }
        });

    pc_->onTrack([self](std::shared_ptr<rtc::Track> track) {
        std::cout << "Got Track event" << std::endl;
        try {
            auto media = track->description();
            rtc::Description::Media::RtpMap* rtp = NULL;
            for (auto pt : media.payloadTypes()) {
                rtc::Description::Media::RtpMap* r = NULL;
                try {
                    r = media.rtpMap(pt);
                } catch (std::exception& ex) {
                    // std::cout << "Bad rtpMap for pt: " << pt << std::endl;
                    continue;
                }
                // TODO: make codec configureable and generalize this matching
                std::string profLvlId = "42e01f";
                // std::string lvlAsymAllowed = "1";
                // std::string pktMode = "1";
                // std::string profLvlId = "4d001f";
                std::string lvlAsymAllowed = "1";
                std::string pktMode = "1";
                if (r != NULL && r->fmtps.size() > 0 &&
                    r->fmtps[0].find("profile-level-id="+profLvlId) != std::string::npos &&
                    r->fmtps[0].find("level-asymmetry-allowed="+lvlAsymAllowed) != std::string::npos &&
                    r->fmtps[0].find("packetization-mode="+pktMode) != std::string::npos
                    ) {
                    std::cout << "FOUND RTP codec match!!! " << pt << std::endl;
                    rtp = r;
                } else {
                    // std::cout << "no match, removing" << std::endl;
                    media.removeRtpMap(pt);
                }
            }
            // TODO: handle no match found error


            // rtc::Description::Media::RtpMap* rtp = media.rtpMap(108);
            // std::cout << "RTP format: " << rtp->format << std::endl
            // << "RTP encParams: " << rtp->encParams << std::endl
            //     << "RTP clockRate: " << rtp->clockRate << std::endl;
            // std::cout << "rtcpFbs: " << std::endl;
            // for (auto fb : rtp->rtcpFbs) {
            //     std::cout << "   " << fb << std::endl;
            // }
            // std::cout << "fmtp: " << std::endl;
            // for (auto fb : rtp->fmtps) {
            //     std::cout << "   " << fb << std::endl;
            // }
            // TODO: not-hardcoded srcPayloadType
            self->srcPayloadType_ = 96;
            self->dstPayloadType_ = rtp->payloadType;
            media.addSSRC(self->ssrc_, "video-send");
            self->track_ = self->pc_->addTrack(media);
        } catch (std::exception ex) {
            std::cout << "GOT EXCEPTION!!! " << ex.what() << std::endl;
        }
    });

    pc_->onDataChannel([self](std::shared_ptr<rtc::DataChannel> incoming) {
        if (incoming->label() == "coap" &&  self->device_ != NULL) {
            self->coapChannel_ = incoming;

            self->nabtoConnection_ = nabto_device_virtual_connection_new(self->device_);

            // TODO: get local/remote description and extract fingerprints from there
            const char* clifp = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
            const char* devFp = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
            nabto_device_virtual_connection_set_client_fingerprint(self->nabtoConnection_, clifp);
            nabto_device_virtual_connection_set_device_fingerprint(self->nabtoConnection_, devFp);

            // TODO: handle allocation error
            self->coapChannel_->onMessage([self](rtc::binary data) {
                std::cout << "Got Data channel binary data"
                    << std::endl;

                },
                [self](std::string data) {
                    std::cout
                        << "Got data channel string data: " << data
                        << std::endl;

                    json message = json::parse(data);
                    if (message["type"].get<int>() == controlMessageType::COAP_REQUEST) {
                        std::cout << "parsed json: " << message.dump() << std::endl;
                        self->coapRequestId_ = message["requestId"].get<std::string>();
                        std::string method = message["method"].get<std::string>();
                        std::string path = message["path"].get<std::string>();

                        std::vector<uint8_t> payload;
                        try {
                            payload = message["payload"]["data"].get<std::vector<uint8_t>>();
                            std::cout << "Got payload of size " << payload.size() << std::endl;

                            for (auto i : payload) {
                                std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)i;
                            }
                            std::cout << std::dec << std::endl;
                        } catch(std::exception& ec) {
                            //ignore
                            std::cout << "Failed to get payload" << std::endl;
                        }

                        uint16_t ct = 0;
                        try {
                            ct = message["contentType"].get<uint16_t>();
                        }
                        catch (std::exception& ec) {
                            //ignore
                        }
                        // TODO: support payloads

                        if (path[0] == '/') {
                            path = path.substr(1);
                        }

                        // TODO: do not just assume max 10 segments
                        const char** pathSegments = (const char**)calloc(10, sizeof(char*));
                        std::vector<std::string> stdSegments;
                        size_t pos = 0;
                        size_t count = 0;
                        pos = path.find("/");
                        while(pos != std::string::npos){
                            std::string segment = path.substr(0,pos);
                            std::cout << "found segment at pos: " << pos << ": " << segment << std::endl;
                            char* s = (char*)calloc(0, segment.length());
                            memcpy(s, segment.c_str(), segment.length());
                            stdSegments.push_back(segment);
                            pathSegments[count] = s;
                            path = path.substr(pos+1);
                            pos = path.find("/");
                            std::cout << "added segment " << pathSegments[count] << " to index " << count << std::endl;
                            count++;
                        }
                        pathSegments[count] = path.c_str();
                        std::cout << "found last segment: " << path << std::endl;
                        std::cout << "added segment " << pathSegments[count] << " to index " << count << std::endl;

                        NabtoDeviceCoapMethod coapMethod;
                        // TODO: support all methods
                        if (method == "GET") {
                            coapMethod = NABTO_DEVICE_COAP_GET;
                        }
                        else if (method == "POST") {
                            coapMethod = NABTO_DEVICE_COAP_POST;
                        }

                        std::cout << "using Segments: ";

                        for (int i = 0; i <= count; i++) {
                            std::cout << "/" << pathSegments[i];
                        }
                        std::cout << std::endl;
                        // TODO: check coap_ is available
                        self->coap_ = nabto_device_virtual_coap_request_new(self->nabtoConnection_, coapMethod, pathSegments);

                        if (!payload.empty()) {
                            std::cout << "Setting payload with content format: " << ct << std::endl;
                            nabto_device_virtual_coap_request_set_payload(self->coap_, payload.data(), payload.size());
                            nabto_device_virtual_coap_request_set_content_format(self->coap_, ct);
                        }

                        NabtoDeviceFuture* fut = nabto_device_future_new(self->device_);
                        nabto_device_virtual_coap_request_execute(self->coap_, fut);

                        nabto_device_future_set_callback(fut, &WebrtcChannel::coapCallback, self.get());

                    }
                });
        }
    });
}


void WebrtcChannel::handleOffer(std::string& offer) {
    std::cout << "Got Offer: " << offer << std::endl;
    if (!pc_) {
        createPeerConnection();
    }
    // TODO: handle json exceptions
    json sdp = json::parse(offer);
    rtc::Description remDesc(sdp["sdp"].get<std::string>(), sdp["type"].get<std::string>());
    try {
    pc_->setRemoteDescription(remDesc);
    } catch (std::invalid_argument& ex) {
        std::cout << "GOT INVALID ARGUMENT: " << ex.what() << std::endl;
    }
}

void WebrtcChannel::handleAnswer(std::string& answer) {
    std::cout << "Got Answer: " << answer << std::endl;
    // TODO: handle json exceptions
    json sdp = json::parse(answer);
    rtc::Description remDesc(sdp["sdp"].get<std::string>(), sdp["type"].get<std::string>());
    pc_->setRemoteDescription(remDesc);
}

void WebrtcChannel::handleIce(std::string& ice) {
    std::cout << "Got ICE: " << ice << std::endl;
    //return;
    // TODO: handle json exceptions
    json candidate = json::parse(ice);
    rtc::Candidate cand(candidate["candidate"].get<std::string>(), candidate["sdpMid"].get<std::string>());
    pc_->addRemoteCandidate(cand);
}


} // Namespace
