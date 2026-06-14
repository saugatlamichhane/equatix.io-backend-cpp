#pragma once
#include <algorithm>
#include <deque>
#include <drogon/WebSocketController.h>
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <json/json.h>
#include <mutex>
#include <unordered_map>

using namespace drogon;

class EloMatchmakingController
    : public WebSocketController<EloMatchmakingController> {
public:
  void handleNewMessage(const WebSocketConnectionPtr &wsConnPtr,
                        std::string &&message,
                        const WebSocketMessageType &type) override;

  void handleConnectionClosed(const WebSocketConnectionPtr &wsConnPtr) override;
  void handleNewConnection(const HttpRequestPtr &,
                           const WebSocketConnectionPtr &) override;

  WS_PATH_LIST_BEGIN
  WS_PATH_ADD("/ws/elo_matchmaking");
  WS_PATH_LIST_END

private:
  struct WaitingEntry {
    std::string userId;
    int elo;
  };

  std::deque<WaitingEntry> waitingQueue_;
  std::unordered_map<std::string, WebSocketConnectionPtr> wsMap_;
  std::mutex queueMutex_;
  static constexpr int ELO_RANGE = 100;
};
