#pragma once
#include <drogon/WebSocketController.h>
#include <drogon/PubSubService.h>
#include <json/json.h>
#include <map>
#include <vector>
#include <string>
#include <memory>

using namespace drogon;

struct RoomState {
    WebSocketConnectionPtr player1Conn;
    WebSocketConnectionPtr player2Conn;
    std::vector<std::string> player1Rack;
    std::vector<std::string> player2Rack;
    std::vector<Json::Value> state_;
    std::vector<Json::Value> current_;
    int score1 = 0;
    int score2 = 0;
    int currentTurn = 1;
    std::vector<std::string> tileBag;
    int passes = 0;
    bool isBot = false;
};

class EchoWebsock : public drogon::WebSocketController<EchoWebsock> {
  public:
    virtual void handleNewMessage(const WebSocketConnectionPtr &wsConnPtr,
                                  std::string &&message,
                                  const WebSocketMessageType &type) override;
    virtual void handleNewConnection(const HttpRequestPtr &req,
                                     const WebSocketConnectionPtr &wsConnPtr) override;
    virtual void handleConnectionClosed(const WebSocketConnectionPtr &wsConnPtr) override;
    
  private:
    PubSubService chatRooms_;
    std::map<std::string, RoomState> rooms;
};
