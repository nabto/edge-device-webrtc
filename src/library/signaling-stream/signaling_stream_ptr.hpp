#pragma once

#include <memory>

namespace nabto {

class SignalingStreamManager;
typedef std::shared_ptr<SignalingStreamManager> SignalingStreamManagerPtr;

class SignalingStream;

typedef std::shared_ptr<SignalingStream> SignalingStreamPtr;
typedef std::weak_ptr<SignalingStream> SignalingStreamWeakPtr;

}
