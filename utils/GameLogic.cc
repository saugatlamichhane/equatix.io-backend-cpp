#include "GameLogic.h"
#include "RoomState.h"
#include <random>
#include <algorithm>
#include "Board.h"

void reset(RoomState &room, int playerTurn) {
  std::vector<std::string> &playerRack =
      (playerTurn == 1) ? room.player1Rack : room.player2Rack;
  for (auto &tile : room.current_) {
    playerRack.push_back(tile["value"].asString());
  }
  room.current_.clear();
  Json::Value response;
  response["type"] = "reset";
  response["rack"] = Json::Value(Json::arrayValue);
  for (auto &tile : playerRack) {
    response["rack"].append(tile);
  }
      if (playerTurn == 1) {
          room.player1Conn->send(

                  Json::writeString(Json::StreamWriterBuilder(), response));
        room.currentTurn = 2;
      } else {
                  room.player2Conn->send(Json::writeString(Json::StreamWriterBuilder(), response));
                  room.currentTurn = 1;

      }
 
}

std::vector<std::string> createTileBag() {
  std::vector<std::string> bag;
  for (auto &[symbol, count] : tileCount) {
    for (int i = 0; i < count; i++) {
      bag.push_back(symbol);
    }
  }
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(bag.begin(), bag.end(), g);
  return bag;
}

std::vector<std::string> drawTiles(std::vector<std::string> &tileBag, int n) {
  if (n > (int)tileBag.size()) {
    n = tileBag.size();
  }
  std::vector<std::string> drawnTiles;
  drawnTiles.insert(drawnTiles.end(), tileBag.end() - n, tileBag.end());
  tileBag.erase(tileBag.end() - n, tileBag.end());
  return drawnTiles;
}
