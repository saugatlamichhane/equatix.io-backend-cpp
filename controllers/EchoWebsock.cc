// EchoWebsock.cc
#include "EchoWebsock.h"
#include <algorithm>
#include <drogon/HttpTypes.h>
#include <drogon/PubSubService.h>
#include <drogon/WebSocketConnection.h>
#include <memory>
#include <random>
#include <stack>
#include <stdexcept>
#include <string>
#include <trantor/utils/Logger.h>
#include <type_traits>
#include <vector>
#include "../utils/Board.h"
#include "../utils/GameLogic.h"
#include "../utils/RoomState.h"
#include "../utils/ValidatorHelpers.h"
#include <variant>


struct Subscriber {
  std::string chatRoomName_;
  drogon::SubscriberID id_;
};


void EchoWebsock::handleNewMessage(const WebSocketConnectionPtr &wsConnPtr,
                                   std::string &&message,
                                   const WebSocketMessageType &type) {
  // write your application logic here
  if (type == WebSocketMessageType::Ping) {
    LOG_DEBUG << "recv a ping";
  } else if (type == WebSocketMessageType::Text) {
    auto &s = wsConnPtr->getContextRef<Subscriber>();
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (!reader->parse(message.data(), message.data() + message.size(), &root,
                       &errs)) {
      LOG_ERROR << "Invalid JSON received: " << errs;
      return;
    }
    Json::Value response;
    auto &room = rooms[s.chatRoomName_];
    std::string msgType = root["type"].asString();
    int playerTurn = (wsConnPtr == room.player1Conn) ? 1 : 2;
    if (checkEndGame(room)) {
      response["type"] = "error";
      response["message"] = "Game Already Over!";
      chatRooms_.publish(
          s.chatRoomName_,
          Json::writeString(Json::StreamWriterBuilder(), response));
      return;
    }
    if ((msgType == "placement" || msgType == "evaluate") &&
        playerTurn != room.currentTurn) {
      response["type"] = "error";
      response["message"] = "Not your turn";
      response["writer"] = playerTurn;
      response["turn"] = room.currentTurn;
      chatRooms_.publish(
          s.chatRoomName_,
          Json::writeString(Json::StreamWriterBuilder(), response));
      return;
    }
    if (msgType == "reset") {
      reset(room, playerTurn);
Json::Value stateResponse;
      stateResponse["type"] = "state";
      stateResponse["tiles"] = Json::Value(Json::arrayValue);
      for (auto &tile : room.state_) {
        stateResponse["tiles"].append(tile);
      }
      stateResponse["Player1 Score"] = room.score1;
      stateResponse["Player2 Score"] = room.score2;
      stateResponse["current tiles"] = Json::Value(Json::arrayValue);
      for (auto &tile : room.current_) {
        stateResponse["current tiles"].append(tile);
      }
      stateResponse["affected"] = Json::Value(Json::arrayValue);
      auto affected = getAffectedEquations(room.state_, room.current_);
      for (const auto &seq : affected) {
        Json::Value arr(Json::arrayValue);
        for (const auto &tile : seq) {
          arr.append(tile);
        }
        stateResponse["affected"].append(arr);
      }
      stateResponse["turn"] = room.currentTurn;
      stateResponse["passes"] = room.passes;

      chatRooms_.publish(
          s.chatRoomName_,
          Json::writeString(Json::StreamWriterBuilder(), stateResponse));
      return;
    } else if (msgType == "swap") {
      room.passes++;
      const Json::Value &tilesToSwap = root["tiles"];
      auto &playerRack =
          (room.currentTurn == 1) ? room.player1Rack : room.player2Rack;
      std::vector<std::string> swapList;
      for (auto &t : tilesToSwap) {
        swapList.push_back(t.asString());
      }
      if (room.tileBag.size() < swapList.size()) {
        response["type"] = "error";
        response["message"] = "Not enough tiles in bag to swap.";
        wsConnPtr->send(
            Json::writeString(Json::StreamWriterBuilder(), response));
        return;
      }
      std::unordered_map<std::string, int> rackCount;
      for (auto &tile : playerRack)
        rackCount[tile]++;
      for (auto &tile : swapList) {
        if (--rackCount[tile] < 0) {
          response["type"] = "error";
          response["message"] =
              "Invalid swap: not enough ' " + tile + " ' in rack.";
          wsConnPtr->send(
              Json::writeString(Json::StreamWriterBuilder(), response));
          return;
        }
      }
      for (auto &tile : swapList) {
        auto it = std::find(playerRack.begin(), playerRack.end(), tile);
        if (it != playerRack.end())
          playerRack.erase(it);
      }
      auto newTiles = drawTiles(room.tileBag, (int)swapList.size());
      playerRack.insert(playerRack.end(), newTiles.begin(), newTiles.end());
      for (auto &tile : swapList) {
        room.tileBag.push_back(tile);
      }
      std::shuffle(room.tileBag.begin(), room.tileBag.end(),
                   std::mt19937{std::random_device{}()});

      if (playerTurn == 2) {
        room.currentTurn = 1;
      } else {
         room.currentTurn = 2;

      }

    } else if (msgType == "pass") {
      reset(room, playerTurn);
      room.passes++;

      if (playerTurn == 2) {
        room.currentTurn = 1;
      } else {
          room.currentTurn = 2;

      }
    } else if (msgType == "evaluate") {
      if (room.state_.empty()) {
        if (!touchesCenter(room.current_)) {

          response["type"] = "error";
          response["message"] = "First move must cover the center (8, 8)";
          Json::StreamWriterBuilder wbuilder;
          std::string jsonStr = Json::writeString(wbuilder, response);
          chatRooms_.publish(s.chatRoomName_, jsonStr);

          return;
        }
      } else {
        if (!touchesExisting(room.state_, room.current_)) {
          response["type"] = "error";
          response["messsage"] = "Move must connect to existing tiles.";
          wsConnPtr->send(
              Json::writeString(Json::StreamWriterBuilder(), response));
          return;
        }
      }
      response["affected"] = Json::Value(Json::arrayValue);
      int currentScore = 0;
      auto affected = getAffectedEquations(room.state_, room.current_);
      for (const auto &seq : affected) {
        std::set<std::pair<int, int>> newlyPlaced;
        for (const auto &tile : seq) {
          for (const auto &placedTile : room.current_) {
            if (tile["row"].asInt() == placedTile["row"].asInt() &&
                tile["col"].asInt() == placedTile["col"].asInt()) {
              newlyPlaced.insert(std::pair<int, int>{
                  placedTile["row"].asInt(), placedTile["col"].asInt()});
            }
          }
        }
        currentScore += scoreEquation(seq, newlyPlaced);

        Json::Value arr(Json::arrayValue);
        for (const auto &tile : seq) {
          arr.append(tile);
        }
        Json::Value affectedEquation;
        affectedEquation["equation"] = arr;
        std::string expr = "";
        for (const auto &tile : seq) {
          expr += tile["value"].asString();
        }
        auto parts = splitEquation(expr);
        if (parts.size() < 2) {
          Json::Value errorResponse;
          errorResponse["type"] = "error";
          errorResponse["message"] =
              "Invalid equation (must contain '='): " + expr;
          chatRooms_.publish(
              s.chatRoomName_,
              Json::writeString(Json::StreamWriterBuilder(), errorResponse));

          return;
        }
        Json::Value expressions(Json::arrayValue);
        auto val = evaluateExpression(parts[0]);
        for (const auto &part : parts) {
          Json::Value expression;
          expression["expr"] = part;
          expression["value"] = evaluateExpression(part);
          if (expression["value"] != val) {
            Json::Value errorResponse;
            errorResponse["type"] = "error";
            errorResponse["message"] = "Equation does not hold: " + expr;
            chatRooms_.publish(
                s.chatRoomName_,
                Json::writeString(Json::StreamWriterBuilder(), errorResponse));

            return;
          }
          expressions.append(expression);
        }
        affectedEquation["expressions"] = expressions;
        response["affected"].append(affectedEquation);
      }
      if (playerTurn == 1) {
        room.score1 += currentScore;
      } else {
        room.score2 += currentScore;
      }
      for (auto &t : room.current_) {
        room.state_.push_back(t);
      }
      room.current_.clear();
      room.passes = 0;
      std::vector<std::string> &playerRack =
          (playerTurn == 1) ? room.player1Rack : room.player2Rack;
      auto newTiles = drawTiles(room.tileBag, 10 - playerRack.size());
      for (auto &tile : newTiles) {
        playerRack.push_back(tile);
      }
      Json::Value rackJson;
      rackJson["type"] = "rack";
      rackJson["rack"] = Json::Value(Json::arrayValue);
      for (auto &item : playerRack) {
        rackJson["rack"].append(item);
      }
      if (playerTurn == 2) {
        room.currentTurn = 1;

                  room.player2Conn->send(Json::writeString(Json::StreamWriterBuilder(), rackJson));

      } else {
                  room.player1Conn->send(Json::writeString(Json::StreamWriterBuilder(), rackJson));
                  room.currentTurn = 2;

      }
      Json::StreamWriterBuilder wbuilder;
      std::string jsonStr = Json::writeString(wbuilder, response);
      chatRooms_.publish(s.chatRoomName_, jsonStr);

    } else if (msgType == "placement") {
      Json::Value payload = root["payload"];
      if (isOccupied(room.state_, room.current_, payload["row"].asInt(),
                     payload["col"].asInt())) {
        response["type"] = "error";
        response["message"] = "Cell already occupied";

        Json::StreamWriterBuilder wbuilder;
        std::string jsonStr = Json::writeString(wbuilder, response);
        chatRooms_.publish(s.chatRoomName_, jsonStr);
        return;
      }
      if (!isStraightLine(room.current_, payload["row"].asInt(),
                          payload["col"].asInt())) {
        response["type"] = "error";
        response["message"] = "Not in straight line.";
        Json::StreamWriterBuilder wbuilder;
        std::string jsonStr = Json::writeString(wbuilder, response);
        chatRooms_.publish(s.chatRoomName_, jsonStr);
        return;
      }
      if (!isContiguous(room.state_, room.current_, payload["row"].asInt(),
                        payload["col"].asInt())) {
        response["type"] = "error";
        response["message"] = "Not contiguous";
        Json::StreamWriterBuilder wbuilder;
        std::string jsonStr = Json::writeString(wbuilder, response);
        chatRooms_.publish(s.chatRoomName_, jsonStr);
        return;
      }
      std::string tileValue = payload["value"].asString();
      auto &rack = (playerTurn == 1) ? room.player1Rack : room.player2Rack;
      auto it = std::find(rack.begin(), rack.end(), tileValue);
      if (it == rack.end()) {
        response["type"] = "error";
        response["message"] = "Tile not in rack.";
        chatRooms_.publish(
            s.chatRoomName_,
            Json::writeString(Json::StreamWriterBuilder(), response));
        return;
      }
      rack.erase(it);
      room.current_.push_back(payload);
    }
    if (playerTurn == 1) {
      auto &rack = room.player1Rack;
      Json::Value msg;
      msg["type"] = "rack";
      msg["rack"] = Json::Value(Json::arrayValue);
      for (auto &item : rack) {
        msg["rack"].append(item);
      }
      room.player1Conn->send(
          Json::writeString(Json::StreamWriterBuilder(), msg));
    } else {
      auto &rack = room.player2Rack;
      Json::Value msg;
      msg["type"] = "rack";
      msg["rack"] = Json::Value(Json::arrayValue);
      for (auto &item : rack) {
        msg["rack"].append(item);
      }
room.player2Conn->send(Json::writeString(Json::StreamWriterBuilder(), msg));


    }

    response["type"] = "state";
    response["tiles"] = Json::Value(Json::arrayValue);
    for (auto &tile : room.state_) {
      response["tiles"].append(tile);
    }
    response["Player1 Score"] = room.score1;
    response["Player2 Score"] = room.score2;
    response["current tiles"] = Json::Value(Json::arrayValue);
    for (auto &tile : room.current_) {
      response["current tiles"].append(tile);
    }
    response["affected"] = Json::Value(Json::arrayValue);
    auto affected = getAffectedEquations(room.state_, room.current_);
    for (const auto &seq : affected) {
      Json::Value arr(Json::arrayValue);
      for (const auto &tile : seq) {
        arr.append(tile);
      }
      response["affected"].append(arr);
    }
    response["turn"] = room.currentTurn;
    response["passes"] = room.passes;
    Json::StreamWriterBuilder wbuilder;
    std::string jsonStr = Json::writeString(wbuilder, response);
    chatRooms_.publish(s.chatRoomName_, jsonStr);
    auto win = checkEndGame(room);
    if (win) {
      Json::Value winResponse;
      winResponse["type"] = "game_over";
      if (room.score1 > room.score2) {
        winResponse["winner"] = 1;
      } else if (room.score2 > room.score1) {
        winResponse["winner"] = 2;
      } else {
        winResponse["winner"] = 0;
      }
      winResponse["score1"] = room.score1;
      winResponse["score2"] = room.score2;

      chatRooms_.publish(
          s.chatRoomName_,
          Json::writeString(Json::StreamWriterBuilder(), winResponse));
    }
  }
}
void EchoWebsock::handleNewConnection(const HttpRequestPtr &req,
                                      const WebSocketConnectionPtr &wsConnPtr) {
  // write your application logic here
  LOG_DEBUG << "new websocket connection!";
  Subscriber s;
  s.chatRoomName_ = req->getParameter("room_name");
  auto params = req->getParameters();

  auto &room = rooms[s.chatRoomName_];

  Json::Value init;
  init["type"] = "init";
  init["turn"] = room.currentTurn;

  init["room name"] = s.chatRoomName_;
  if (!room.player1Conn) {
    room.tileBag = createTileBag();
    room.player1Conn = wsConnPtr;
    init["rack"] = Json::Value(Json::arrayValue);
    auto rack = drawTiles(room.tileBag, 10);
    room.player1Rack = rack;
    for (auto tile : rack) {
      init["rack"].append(tile);
    }
    init["sent"] = 1;
    room.player2Conn = nullptr;
  } else if (room.player2Conn == nullptr) {
    room.player2Conn = wsConnPtr;
    init["rack"] = Json::Value(Json::arrayValue);
    auto rack = drawTiles(room.tileBag, 10);
    room.player2Rack = rack;
    for (auto tile : rack) {
      init["rack"].append(tile);
    }
    init["sent"] = 2;
  }
  else {

    init["error"] = "2 Players already connected";
    wsConnPtr->send(Json::writeString(Json::StreamWriterBuilder(), init));
    s.id_ = chatRooms_.subscribe(
        s.chatRoomName_,
        [wsConnPtr](const std::string &topic, const std::string &message) {
          (void)topic;
          wsConnPtr->send(message);
        });
    wsConnPtr->setContext(std::make_shared<Subscriber>(std::move(s)));
    wsConnPtr->forceClose();
    return;
  } 

  
  s.id_ = chatRooms_.subscribe(
      s.chatRoomName_,
      [wsConnPtr](const std::string &topic, const std::string &message) {
        (void)topic;
        wsConnPtr->send(message);
      });
  wsConnPtr->setContext(std::make_shared<Subscriber>(std::move(s)));
  initBoardMultipliers();
  wsConnPtr->send(Json::writeString(Json::StreamWriterBuilder(), init));
}
void EchoWebsock::handleConnectionClosed(
    const WebSocketConnectionPtr &wsConnPtr) {
  // write your application logic here
  LOG_DEBUG << "websocket closed!";
  auto &s = wsConnPtr->getContextRef<Subscriber>();
  chatRooms_.unsubscribe(s.chatRoomName_, s.id_);
  
  // Clear the room connection to allow new players to join
  auto &room = rooms[s.chatRoomName_];
  
  // Check if this was player1 or player2
  if (room.player1Conn == wsConnPtr) {
    room.player1Conn = nullptr;
  } else if (room.player2Conn == wsConnPtr) {
      room.player2Conn = nullptr;
  }
  
  // If both players have disconnected, reset the entire room
  if (room.player1Conn == nullptr && 
      room.player2Conn == nullptr) {
    room = RoomState{}; // Reset to fresh state
  }
}
