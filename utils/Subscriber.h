#pragma once

#include <drogon/PubSubService.h>
#include <drogon/WebSocketConnection.h>
#include <string>
struct Subscriber {
  std::string chatRoomName_;
  drogon::SubscriberID id_;
};