#include "webrtc_coap_channel.hpp"

#include <nlohmann/json.hpp>
#include <iomanip> // For std::setfill and std::setw

namespace nabto {

class VirtualCoapRequest : public std::enable_shared_from_this<VirtualCoapRequest> {
public:
    VirtualCoapRequest(NabtoDevicePtr device, NabtoDeviceVirtualConnection* nabtoConnection, EventQueuePtr queue)
        : device_(device), nabtoConnection_(nabtoConnection), queue_(queue)
    {

    }

    ~VirtualCoapRequest()
    {
    }

    bool createRequest(nlohmann::json& request, std::function<void (const nlohmann::json& response)> responeReady)
    {
        NPLOGD << "parsed json: " << request.dump();
        try {
            coapRequestId_ = request["requestId"].get<std::string>();
            std::string methodStr = request["method"].get<std::string>();

            if (methodStr == "GET") {
                method_ = NABTO_DEVICE_COAP_GET;
            }
            else if (methodStr == "POST") {
                method_ = NABTO_DEVICE_COAP_POST;
            }
            else if (methodStr == "PUT") {
                method_ = NABTO_DEVICE_COAP_PUT;
            }
            else if (methodStr == "DELETE") {
                method_ = NABTO_DEVICE_COAP_DELETE;
            }
            else {
                NPLOGE << "Invalid coap method string: " << methodStr << "Expected one of: GET, POST, PUT, DELETE";
                errorResponse_ = "Invalid method";
                return false;
            }

            std::string path = request["path"].get<std::string>();

            parsePayload(request);
            coap_ = nabto_device_virtual_coap_request_new(nabtoConnection_, method_, path.c_str());

            if (!payload_.empty()) {
                NPLOGD << "Setting payload with content format: " << contentType_;
                nabto_device_virtual_coap_request_set_payload(coap_, payload_.data(), payload_.size());
                nabto_device_virtual_coap_request_set_content_format(coap_, contentType_);
            }

            NabtoDeviceFuture* fut = nabto_device_future_new(device_.get());
            nabto_device_virtual_coap_request_execute(coap_, fut);

            me_ = shared_from_this();
            responeReady_ = responeReady;
            nabto_device_future_set_callback(fut, coapCallback, this);
        }
        catch (nlohmann::json::exception& ex) {
            NPLOGE << "coap createRequest json exception: " << ex.what();
            return false;
        }
        return true;
    }

    nlohmann::json getErrorResponse() {
        nlohmann::json resp = {
           {"type", 1},
           {"requestId", coapRequestId_},
           {"error", errorResponse_}
        };
        return resp;
    }

private:

    void parsePayload(nlohmann::json& request)
    {
        try {
            payload_ = request["payload"]["data"].get<std::vector<uint8_t>>();
            NPLOGD << "Got payload of size " << payload_.size();

            // for (auto i : payload_) {
            //     std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)i;
            // }
            // std::cout << std::dec << std::endl;
            contentType_ = request["contentType"].get<uint16_t>();

        }
        catch (std::exception& ec) {
            // there was no payload
            NPLOGD << "Failed to get payload";

        }
    }

    static void coapCallback(NabtoDeviceFuture* fut, NabtoDeviceError err, void* data)
    {
        VirtualCoapRequest* self = (VirtualCoapRequest*)(data);
        nabto_device_future_free(fut);
        self->queue_->post([self, err]() {
            self->handleCoapResponse(err);
        });

    }

    void handleCoapResponse(NabtoDeviceError err)
    {
        if (err != NABTO_DEVICE_EC_OK) {
            NPLOGD << "Coap request failed with error: " << nabto_device_error_get_message(err);
            nabto_device_virtual_coap_request_free(coap_);
            responeReady_ = nullptr;
            me_ = nullptr;
            return;
        }
        uint16_t status;
        nabto_device_virtual_coap_request_get_response_status_code(coap_, &status);
        uint16_t cf;
        nabto_device_virtual_coap_request_get_response_content_format(coap_, &cf);

        uint8_t* payload;
        size_t len;
        nabto_device_virtual_coap_request_get_response_payload(coap_, (void**)&payload, &len);
        std::vector<uint8_t> respPayload(payload, payload + len);

        nlohmann::json resp = {
            {"type", 1},
            {"requestId", coapRequestId_},
            {"statusCode", status},
            {"contentType", cf},
            {"payload", respPayload}
        };
        responeReady_(resp);
        nabto_device_virtual_coap_request_free(coap_);
        responeReady_ = nullptr;
        me_ = nullptr;
    }

    NabtoDevicePtr device_;
    EventQueuePtr queue_;
    NabtoDeviceVirtualConnection* nabtoConnection_;
    std::string coapRequestId_;
    NabtoDeviceCoapMethod method_;
    std::vector<uint8_t> payload_;
    uint16_t contentType_;

    std::string errorResponse_;

    std::function<void(const nlohmann::json& response)> responeReady_;

    NabtoDeviceVirtualCoapRequest* coap_ = NULL;

    nlohmann::json response_;
    std::shared_ptr<VirtualCoapRequest> me_;
};



WebrtcCoapChannelPtr WebrtcCoapChannel::create(std::shared_ptr<rtc::PeerConnection> pc, std::shared_ptr<rtc::DataChannel> channel, NabtoDevicePtr device, NabtoDeviceVirtualConnection* nabtoConnection, EventQueuePtr queue)
{
    auto ptr = std::make_shared<WebrtcCoapChannel>(pc, channel, device, nabtoConnection, queue);
    ptr->init();
    return ptr;
}

WebrtcCoapChannel::WebrtcCoapChannel(std::shared_ptr<rtc::PeerConnection> pc, std::shared_ptr<rtc::DataChannel> channel, NabtoDevicePtr device, NabtoDeviceVirtualConnection* nabtoConnection, EventQueuePtr queue)
    : pc_(pc), channel_(channel), device_(device), nabtoConnection_(nabtoConnection), queue_(queue)
{
}


void WebrtcCoapChannel::init()
{
    auto self = shared_from_this();
    auto remoteDescription = pc_->remoteDescription();
    auto localDescription = pc_->localDescription();
    if (!remoteDescription || !localDescription) {
        NPLOGE << "Missing local or remote description";
        return;
    }
    auto remoteFingerprint = pc_->remoteDescription()->fingerprint();
    auto localFingerprint = pc_->localDescription()->fingerprint();
    if (!remoteFingerprint || !localFingerprint) {
        NPLOGE << "Missing local or remote fingerprint";
        return;
    }
    std::string cliFp = remoteFingerprint->value;
    std::string devFp = localFingerprint->value;
    cliFp.erase(std::remove(cliFp.begin(), cliFp.end(), ':'), cliFp.end());
    devFp.erase(std::remove(devFp.begin(), devFp.end(), ':'), devFp.end());

    NPLOGD << "Client FP: " << cliFp;
    NPLOGD << "Device FP: " << devFp;

    nabto_device_virtual_connection_set_client_fingerprint(nabtoConnection_, cliFp.c_str());
    nabto_device_virtual_connection_set_device_fingerprint(nabtoConnection_, devFp.c_str());

    channel_->onMessage([self](rtc::binary data) {
        NPLOGD << "Got Data channel binary data"
           ;

        },
        [self](std::string data) {
            NPLOGD
                << "Got data channel string data: " << data
               ;
            self->handleStringMessage(data);
        });
    channel_->onClosed([self]() {
        NPLOGD << "Coap Channel Closed";
        self->channel_ = nullptr;
    });
}

void WebrtcCoapChannel::handleStringMessage(const std::string& data)
{
    auto self = shared_from_this();
    try {
        nlohmann::json message = nlohmann::json::parse(data);
        int type = message["type"].get<int>();
        if (type == coapMessageType::COAP_REQUEST) {
            std::shared_ptr<VirtualCoapRequest> req = std::make_shared<VirtualCoapRequest>(device_, nabtoConnection_, queue_);
            if (!req->createRequest(message, [self, req](const nlohmann::json& response) {
                self->sendResponse(response);
                })) {
                sendResponse(req->getErrorResponse());
            }
        }
        else {
            NPLOGE << "unhandled message type: " << type;
        }
    }
    catch (nlohmann::json::exception& ex) {
        NPLOGE << "coapChannel handleStringmessage json exception: " << ex.what();
    }
}

void WebrtcCoapChannel::sendResponse(const nlohmann::json& response)
{
    try {
        channel_->send(response.dump());
    } catch(std::exception& ex) {
        NPLOGE << "Failed to send Coap Response to data channel: " << ex.what();
    }
}


} // Namespace
