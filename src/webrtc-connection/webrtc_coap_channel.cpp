#include "webrtc_coap_channel.hpp"

#include <nlohmann/json.hpp>
#include <iomanip> // For std::setfill and std::setw

namespace nabto {

class VirtualCoapRequest : public std::enable_shared_from_this<VirtualCoapRequest> {
public:
    VirtualCoapRequest(NabtoDeviceImplPtr device, NabtoDeviceVirtualConnection* nabtoConnection)
        : device_(device), nabtoConnection_(nabtoConnection)
    {

    }

    ~VirtualCoapRequest()
    {
        std::cout << "VirtualCoapRequest Destructor" << std::endl;
    }

    bool createRequest(nlohmann::json& request, std::function<void (const nlohmann::json& response)> responeReady)
    {
        std::cout << "parsed json: " << request.dump() << std::endl;
        // TODO: json exceptions
        coapRequestId_ = request["requestId"].get<std::string>();
        std::string methodStr = request["method"].get<std::string>();

        if (methodStr == "GET") {
            method_ = NABTO_DEVICE_COAP_GET;
        } else if (methodStr == "POST") {
            method_ = NABTO_DEVICE_COAP_POST;
        } else if (methodStr == "PUT") {
            method_ = NABTO_DEVICE_COAP_PUT;
        } else if (methodStr == "DELETE") {
            method_ = NABTO_DEVICE_COAP_DELETE;
        } else {
            std::cout << "Invalid coap method string: " << methodStr << std::endl << "Expected one of: GET, POST, PUT, DELETE" << std::endl;
            errorResponse_ = "Invalid method";
            return false;
        }

        std::string path = request["path"].get<std::string>();

        const char** segments = parsePathSegments(path);
        if (segments == NULL) {
            errorResponse_ = "Out of memory";
            return false;
        }
        size_t i = 0;
        const char* s;
        std::cout << "Parsed segments: ";

        while((s = segments[i]) != NULL) {
            std::cout << "/" << s;
            i++;
        }
        std::cout << std::endl;

        parsePayload(request);

        coap_ = nabto_device_virtual_coap_request_new(nabtoConnection_, method_, segments);

        if (!payload_.empty()) {
            std::cout << "Setting payload with content format: " << contentType_ << std::endl;
            nabto_device_virtual_coap_request_set_payload(coap_, payload_.data(), payload_.size());
            nabto_device_virtual_coap_request_set_content_format(coap_, contentType_);
        }

        NabtoDeviceFuture* fut = nabto_device_future_new(device_->getDevice());
        nabto_device_virtual_coap_request_execute(coap_, fut);

        me_ = shared_from_this();
        nabto_device_future_set_callback(fut, coapCallback, this);
        responeReady_ = responeReady;
        free(segments);
        for (auto s : pathSegments_) {
            free((void*)s);
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
            std::cout << "Got payload of size " << payload_.size() << std::endl;

            for (auto i : payload_) {
                std::cout << std::setfill('0') << std::setw(2) << std::hex << (int)i;
            }
            std::cout << std::dec << std::endl;
            contentType_ = request["contentType"].get<uint16_t>();

        }
        catch (std::exception& ec) {
            // there was no payload
            std::cout << "Failed to get payload" << std::endl;

        }
    }

    const char** parsePathSegments(std::string& inPath)
    {
        std::string path = inPath;
        if (path[0] == '/') {
            path = path.substr(1);
        }

        size_t pos = 0;
        size_t count = 0;
        pos = path.find("/");
        while (pos != std::string::npos) {
            std::string segment = path.substr(0, pos);
            std::cout << "found segment at pos: " << pos << ": " << segment << std::endl;
            char* s = strdup(segment.c_str());
            // char* s = (char*)calloc(0, segment.length()+1);
            // memcpy(s, segment.c_str(), segment.length());
            pathSegments_.push_back(s);
            path = path.substr(pos + 1);
            pos = path.find("/");
            count++;
        }

        char* s = strdup(path.c_str());
        // char* s = (char*)calloc(0, path.length());
        // memcpy(s, path.c_str(), path.length());
        pathSegments_.push_back(s);
        count++;
        std::cout << "found last segment: " << path << std::endl;

        const char** result = (const char**)calloc(count + 1, sizeof(char*));
        if (result == NULL) {
            return NULL;
        }
        for (size_t i = 0; i < count; i++) {
            result[i] = pathSegments_[i];
        }
        return result;

    }


    static void coapCallback(NabtoDeviceFuture* fut, NabtoDeviceError err, void* data)
    {
        VirtualCoapRequest* self = (VirtualCoapRequest*)(data);
        nabto_device_future_free(fut);
        self->handleCoapResponse(err);

    }

    void handleCoapResponse(NabtoDeviceError err)
    {
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

    NabtoDeviceImplPtr device_;
    NabtoDeviceVirtualConnection* nabtoConnection_;
    std::string coapRequestId_;
    NabtoDeviceCoapMethod method_;
    std::vector<const char*> pathSegments_;
    std::vector<uint8_t> payload_;
    uint16_t contentType_;

    std::string errorResponse_;

    std::function<void(const nlohmann::json& response)> responeReady_;

    NabtoDeviceVirtualCoapRequest* coap_ = NULL;

    nlohmann::json response_;
    std::shared_ptr<VirtualCoapRequest> me_;
};



WebrtcCoapChannelPtr WebrtcCoapChannel::create(std::shared_ptr<rtc::DataChannel> channel, NabtoDeviceImplPtr device, NabtoDeviceVirtualConnection* nabtoConnection)
{
    auto ptr = std::make_shared<WebrtcCoapChannel>(channel, device, nabtoConnection);
    ptr->init();
    return ptr;
}

WebrtcCoapChannel::WebrtcCoapChannel(std::shared_ptr<rtc::DataChannel> channel, NabtoDeviceImplPtr device, NabtoDeviceVirtualConnection* nabtoConnection)
    : channel_(channel), device_(device), nabtoConnection_(nabtoConnection)
{
}


void WebrtcCoapChannel::init()
{
    auto self = shared_from_this();
    // TODO: get local/remote description and extract fingerprints from there
    const char* clifp = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    const char* devFp = "abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789";
    nabto_device_virtual_connection_set_client_fingerprint(nabtoConnection_, clifp);
    nabto_device_virtual_connection_set_device_fingerprint(nabtoConnection_, devFp);

    channel_->onMessage([self](rtc::binary data) {
        std::cout << "Got Data channel binary data"
            << std::endl;

        },
        [self](std::string data) {
            std::cout
                << "Got data channel string data: " << data
                << std::endl;
            self->handleStringMessage(data);
        });
    channel_->onClosed([self]() {
        std::cout << "Coap Channel Closed" << std::endl;
        self->channel_ = nullptr;
    });
}

void WebrtcCoapChannel::handleStringMessage(std::string& data)
{
    auto self = shared_from_this();
    // TODO: json execptions
    nlohmann::json message = nlohmann::json::parse(data);
    int type = message["type"].get<int>();
    if (type == coapMessageType::COAP_REQUEST) {
        std::shared_ptr<VirtualCoapRequest> req = std::make_shared<VirtualCoapRequest>(device_, nabtoConnection_);
        if (!req->createRequest(message, [self, req](const nlohmann::json& response) {
            self->sendResponse(response);
        })) {
            sendResponse(req->getErrorResponse());
        }
    } else {
        std::cout << "unhandled message type: " << type << std::endl;
    }

}

void WebrtcCoapChannel::sendResponse(const nlohmann::json& response)
{
    try {
        channel_->send(response.dump());
    } catch(std::exception& ex) {
        std::cout << "Failed to send Coap Response to data channel: " << ex.what() << std::endl;
    }
}


} // Namespace
