#pragma once

#include <string>
#include <drogon/WebSocketConnection.h>
#include <drogon/PubSubService.h>
struct Subscriber {
  std::string chatRoomName_;
  drogon::SubscriberID id_;
};