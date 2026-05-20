#pragma once

#include <drogon/WebSocketConnection.h>
#include <drogon/WebSocketController.h>
#include <json/json.h>
#include <string>
#include <variant>
#include <vector>

using namespace drogon;

struct MoveRecord {
  int playerSide;
  std::vector<Json::Value> tiles;
  int scoreGained;
  std::string timestamp;

};

struct RoomState {
  std::vector<Json::Value> state_;
  std::vector<Json::Value> current_;
  WebSocketConnectionPtr player1Conn;
  WebSocketConnectionPtr player2Conn;
  std::string player1Uid;
  std::string player2Uid;
  int currentTurn = 1;
  std::vector<std::string> tileBag;
  std::vector<std::string> player1Rack;
  std::vector<std::string> player2Rack;
  int score1 = 0;
  int score2 = 0;
  int passes = 0;
  int challengeId = -1;
  trantor::TimerId activeTimerId = 0;
  int p1Timeouts = 0;
  int p2Timeouts = 0;
  bool isP1Connected = true;
  bool isP2Connected = true;
  bool isBotGame = false;
  static constexpr double TURN_TIME_LIMIT = 45.0; // seconds
  static constexpr double RECONNECT_TIME_LIMIT = 30.0; // seconds
  static constexpr int BOARD_SIZE = 15;
  static constexpr int RACK_SIZE = 7;
  static constexpr int CENTER = 8;
  std::vector<MoveRecord> moveHistory;
};
