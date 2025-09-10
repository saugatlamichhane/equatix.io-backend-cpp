#include "GameLogic.h"
#include <algorithm>
#include <random>
#include <drogon/WebSocketConnection.h>
#include <json/json.h>

bool checkEndGame(RoomState &room){
    return (room.tileBag.empty() &&
           (room.player1Rack.empty() || room.player2Rack.empty())) || room.passes==6;
}

void reset(RoomState &room, int playerTurn){
    std::vector<std::string> &playerRack = (playerTurn==1) ? room.player1Rack : room.player2Rack;
    for(auto &tile : room.current_) playerRack.push_back(tile["value"].asString());
    room.current_.clear();
    Json::Value response;
    response["type"]="reset";
    response["rack"]=Json::Value(Json::arrayValue);
    for(auto &tile : playerRack) response["rack"].append(tile);
    WebSocketConnectionPtr wsConnPtr = (playerTurn==1) ? room.player1Conn : room.player2Conn;
    if(wsConnPtr) wsConnPtr->send(Json::writeString(Json::StreamWriterBuilder(), response));
}
