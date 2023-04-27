#pragma once

#include <iostream>

namespace nabto {

class WebrtcChannel {
public:
    WebrtcChannel() {

    }

    void handleOffer(std::string& offer) {
        std::cout << "Got Offer: " << offer << std::endl;
    }

    void handleAnswer(std::string& offer) {
        std::cout << "Got Answer: " << offer << std::endl;

    }

    void handleIce(std::string& offer) {
        std::cout << "Got ICE: " << offer << std::endl;

    }

    void handleTurnReq() {
        std::cout << "Got Turn Req" << std::endl;

    }

};


}
