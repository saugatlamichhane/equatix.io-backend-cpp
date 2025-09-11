#include <json/json.h>
#include <drogon/WebSocketConnection.h>
#include <vector>
#include <string>

struct RoomState {
  std::vector<Json::Value> state_;
  std::vector<Json::Value> current_;
  WebSocketConnectionPtr player1Conn;
  WebSocketConnectionPtr player2Conn;
  int currentTurn = 1;
  std::vector<std::string> tileBag;
  std::vector<std::string> player1Rack;
  std::vector<std::string> player2Rack;
  int score1 = 0;
  int score2 = 0;
  int passes = 0;
  bool isBot = false;
};