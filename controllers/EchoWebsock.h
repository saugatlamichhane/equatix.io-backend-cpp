//EchoWebsock.h
#pragma once
#include <drogon/WebSocketController.h>
#include <drogon/PubSubService.h>
#include <drogon/HttpAppFramework.h>
using namespace drogon;

struct RoomState {
    std::vector<Json::Value> state_;
    std::vector<Json::Value> current_;
    WebSocketConnectionPtr player1Conn;
    WebSocketConnectionPtr player2Conn;
    int currentTurn = 1;
};

class EchoWebsock:public drogon::WebSocketController<EchoWebsock>
{
public:
    virtual void handleNewMessage(const WebSocketConnectionPtr&,
                                std::string &&,
                                const WebSocketMessageType &)override;
    virtual void handleNewConnection(const HttpRequestPtr &,
                                    const WebSocketConnectionPtr&)override;
    virtual void handleConnectionClosed(const WebSocketConnectionPtr&)override;
    WS_PATH_LIST_BEGIN
    //list path definitions here;
    WS_PATH_ADD("/echo");
    WS_PATH_LIST_END
private:
        PubSubService<std::string> chatRooms_;
        std::unordered_map<std::string, RoomState> rooms;
};
