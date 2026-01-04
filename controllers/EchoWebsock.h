// EchoWebsock.h
#pragma once
#include <drogon/HttpAppFramework.h>
#include <drogon/PubSubService.h>
#include <drogon/WebSocketController.h>
#include <unordered_map>
#include <utils/RoomState.h>
using namespace drogon;

class EchoWebsock : public drogon::WebSocketController<EchoWebsock> {
public:
  virtual void handleNewMessage(const WebSocketConnectionPtr &, std::string &&,
                                const WebSocketMessageType &) override;
  virtual void handleNewConnection(const HttpRequestPtr &,
                                   const WebSocketConnectionPtr &) override;
  virtual void handleConnectionClosed(const WebSocketConnectionPtr &) override;
  WS_PATH_LIST_BEGIN
  // list path definitions here;
  WS_PATH_ADD("/echo");
  WS_PATH_LIST_END
private:
  PubSubService<std::string> chatRooms_;
  std::unordered_map<std::string, RoomState> rooms;
  void startTurnTimer(const std::string &roomName);
  void broadcastState(const std::string &roomName);
  void handleForfeit(const std::string &roomName, int winnerSide,
                     const std::string &reason);
  void stopTimer(const std::string &roomName);
  void applyGameRewards(const std::string &winnerUid,
                        const std::string &loserUid, bool isForfeit);
  void executeBotMove(const std::string &roomName);
};