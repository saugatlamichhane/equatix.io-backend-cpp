//EchoWebsock.cc
#include "EchoWebsock.h"
#include <drogon/HttpTypes.h>
#include <drogon/PubSubService.h>
#include <drogon/WebSocketConnection.h>
#include <memory>
#include <trantor/utils/Logger.h>

struct Subscriber {
    std::string chatRoomName_;
    drogon::SubscriberID id_;
};

void EchoWebsock::handleNewMessage(const WebSocketConnectionPtr &wsConnPtr,std::string &&message, const WebSocketMessageType& type)
{
    //write your application logic here
    LOG_DEBUG << "new websocket message: " << message;
    if(type == WebSocketMessageType::Ping) {
        LOG_DEBUG << "recv a ping";
    } else if(type == WebSocketMessageType::Text) {
        auto &s = wsConnPtr->getContextRef<Subscriber>();
        Json::Value response;
        response["type"] = "message";
        response["message"] = message;
        Json::StreamWriterBuilder wbuilder;
        std::string jsonStr = Json::writeString(wbuilder, response);
        chatRooms_.publish(s.chatRoomName_, jsonStr);
    }
}
void EchoWebsock::handleNewConnection(const HttpRequestPtr &req,const WebSocketConnectionPtr &wsConnPtr)
{
    //write your application logic here
    LOG_DEBUG << "new websocket connection!";

    wsConnPtr->send("haha!!");
    Subscriber s;
    s.chatRoomName_ = req->getParameter("room_name");
    s.id_ = chatRooms_.subscribe(s.chatRoomName_, [wsConnPtr] (const std::string &topic, const std::string &message) {
            (void) topic;
            wsConnPtr->send(message);
            });
    wsConnPtr->setContext(std::make_shared<Subscriber>(std::move(s)));
}
void EchoWebsock::handleConnectionClosed(const WebSocketConnectionPtr &wsConnPtr)
{
    //write your application logic here
    LOG_DEBUG << "websocket closed!";
    auto &s = wsConnPtr->getContextRef<Subscriber>();
    chatRooms_.unsubscribe(s.chatRoomName_, s.id_);
}
