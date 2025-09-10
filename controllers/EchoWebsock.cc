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
#include <vector>

struct Subscriber {
  std::string chatRoomName_;
  drogon::SubscriberID id_;
};

enum class MultiplierType {
  NONE,
  DOUBLE_TILE,
  TRIPLE_TILE,
  DOUBLE_EQUATION,
  TRIPLE_EQUATION
};

std::map<std::pair<int, int>, MultiplierType> boardMultipliers;

void initBoardMultipliers() {
  // 1️⃣ Initialize all cells to NONE
  for (int r = 1; r <= 15; ++r) {
    for (int c = 1; c <= 15; ++c) {
      boardMultipliers[{r, c}] = MultiplierType::NONE;
    }
  }

  // 2️⃣ Triple Equation (Triple Word)
  std::pair<int, int> tripleWordCoords[] = {
      {1, 1}, {1, 8}, {1, 15}, {8, 1}, {8, 15}, {15, 1}, {15, 8}, {15, 15}};
  for (auto &coord : tripleWordCoords)
    boardMultipliers[coord] = MultiplierType::TRIPLE_EQUATION;

  // 3️⃣ Double Equation (Double Word)
  std::pair<int, int> doubleWordCoords[] = {
      {2, 2},   {2, 14}, {3, 3},   {3, 13}, {4, 4},   {4, 12},
      {5, 5},   {5, 11}, {8, 8},   {11, 5}, {11, 11}, {12, 4},
      {12, 12}, {13, 3}, {13, 13}, {14, 2}, {14, 14}};
  for (auto &coord : doubleWordCoords)
    boardMultipliers[coord] = MultiplierType::DOUBLE_EQUATION;

  // 4️⃣ Triple Tile (Triple Letter)
  std::pair<int, int> tripleTileCoords[] = {
      {2, 6},  {2, 10}, {6, 2},   {6, 6},   {6, 10}, {6, 14},
      {10, 2}, {10, 6}, {10, 10}, {10, 14}, {14, 6}, {14, 10}};
  for (auto &coord : tripleTileCoords)
    boardMultipliers[coord] = MultiplierType::TRIPLE_TILE;

  // 5️⃣ Double Tile (Double Letter)
  std::pair<int, int> doubleTileCoords[] = {
      {1, 4},  {1, 12}, {3, 7},  {3, 9},   {4, 1},  {4, 8},  {4, 15}, {7, 3},
      {7, 7},  {7, 9},  {7, 13}, {8, 4},   {8, 12}, {9, 3},  {9, 7},  {9, 9},
      {9, 13}, {12, 1}, {12, 8}, {12, 15}, {13, 7}, {13, 9}, {15, 4}, {15, 12}};
  for (auto &coord : doubleTileCoords)
    boardMultipliers[coord] = MultiplierType::DOUBLE_TILE;
}

std::map<std::string, int> tileCount{{"0", 1}, {"1", 1}, {"2", 1}, {"3", 1},
                                     {"4", 1}, {"5", 1}, {"6", 1}, {"7", 1},
                                     {"8", 1}, {"9", 1}, {"-", 2}, {"+", 2},
                                     {"/", 2}, {"*", 2}, {"=", 8}

};

std::map<std::string, int> tilePoints = {{"0", 5}, {"1", 5}, {"2", 5}, {"3", 5},
                                         {"4", 5}, {"5", 5}, {"6", 5}, {"7", 5},
                                         {"8", 5}, {"9", 5}, {"-", 5}, {"+", 5},
                                         {"/", 5}, {"*", 5}, {"=", 5}};

int scoreEquation(const std::vector<Json::Value> &equation,
                  const std::set<std::pair<int, int>> &newlyPlaced) {
  int wordMultiplier = 1;
  int score = 0;

  for (auto &tile : equation) {
    int r = tile["row"].asInt();
    int c = tile["col"].asInt();
    std::string val = tile["value"].asString();

    int letterScore = tilePoints.at(val);
    std::pair<int, int> pos = {r, c};

    if (newlyPlaced.count(pos)) {
      switch (boardMultipliers[pos]) {
      case MultiplierType::DOUBLE_TILE:
        letterScore *= 2;
        break;
      case MultiplierType::TRIPLE_TILE:
        letterScore *= 3;
        break;
      case MultiplierType::DOUBLE_EQUATION:
        wordMultiplier *= 2;
        break;
      case MultiplierType::TRIPLE_EQUATION:
        wordMultiplier *= 3;
        break;
      default:
        break;
      }
    }
    score += letterScore;
  }
  return score * wordMultiplier;
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

bool touchesCenter(const std::vector<Json::Value> &current) {
  for (auto &t : current) {
    if (t["row"].asInt() == 8 && t["col"].asInt() == 8) {
      return true;
    }
  }
  return false;
}

bool touchesExisting(const std::vector<Json::Value> &state,
                     const std::vector<Json::Value> &current) {
  std::set<std::pair<int, int>> placed;
  for (auto &t : current) {

    placed.insert({t["row"].asInt(), t["col"].asInt()});
  }
  for (auto &t : current) {
    int r = t["row"].asInt();
    int c = t["col"].asInt();
    std::vector<std::pair<int, int>> neighbors = {
        {r - 1, c}, {r + 1, c}, {r, c - 1}, {r, c + 1}};
    for (auto &nb : neighbors) {
      for (auto &s : state) {
        if (s["row"].asInt() == nb.first && s["col"].asInt() == nb.second) {
          return true;
        }
      }
    }
  }
  return false;
}

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
  WebSocketConnectionPtr wsConnPtr =
      (playerTurn == 1) ? room.player1Conn : room.player2Conn;
  if (wsConnPtr) {
    wsConnPtr->send(Json::writeString(Json::StreamWriterBuilder(), response));
  }
}

bool isOccupied(std::vector<Json::Value> current_,
                std::vector<Json::Value> state_, int row, int col) {
  for (auto &item : current_) {
    if (item["row"].asInt() == row && item["col"].asInt() == col) {
      return true;
    }
  }
  for (auto &item : state_) {
    if (item["row"].asInt() == row && item["col"].asInt() == col) {
      return true;
    }
  }
  return false;
}

bool isStraightLine(std::vector<Json::Value> current_, int row, int col) {
  if (current_.empty())
    return true;
  bool isHorizontal = true;
  int firstRow = current_.at(0)["row"].asInt();
  for (const auto &tile : current_) {
    if (tile["row"].asInt() != firstRow) {
      isHorizontal = false;
      break;
    }
  }
  if (row != firstRow)
    isHorizontal = false;

  bool isVertical = true;
  int firstCol = current_[0]["col"].asInt();

  for (const auto &tile : current_) {
    if (tile["col"].asInt() != firstCol) {
      isVertical = false;
      break;
    }
  }
  if (col != firstCol)
    isVertical = false;
  return isHorizontal || isVertical;
}

bool isContiguous(std::vector<Json::Value> state,
                  std::vector<Json::Value> current, int row, int col) {
  if (current.empty())
    return true;
  bool sameRow =
      std::all_of(current.begin(), current.end(),
                  [&](const auto &tile) { return tile["row"].asInt() == row; });
  bool sameCol =
      std::all_of(current.begin(), current.end(),
                  [&](const auto &tile) { return tile["col"].asInt() == col; });
  if (!sameRow && !sameCol)
    return false;

  if (sameRow) {
    std::set<int> cols;
    for (const auto &tile : current) {
      if (tile["row"].asInt() == row)
        cols.insert(tile["col"].asInt());
    }
    cols.insert(col);
    for (const auto &tile : state) {
      if (tile["row"].asInt() == row)
        cols.insert(tile["col"].asInt());
    }
    int minCol = 20, maxCol = -1;
    for (const auto &tile : current) {
      minCol = std::min(minCol, tile["col"].asInt());
      maxCol = std::max(maxCol, tile["col"].asInt());
    }
    minCol = std::min(minCol, col);
    maxCol = std::max(maxCol, col);
    for (int c = minCol; c <= maxCol; ++c) {
      if (cols.find(c) == cols.end()) {
        return false;
      }
    }
  } else if (sameCol) {
    std::set<int> rows;
    for (const auto &tile : current) {
      if (tile["col"].asInt() == col)
        rows.insert(tile["row"].asInt());
    }
    rows.insert(row);
    for (const auto &tile : state) {
      if (tile["col"].asInt() == col)
        rows.insert(tile["row"].asInt());
    }
    int minRow = 20, maxRow = -1;
    for (const auto &tile : current) {
      minRow = std::min(minRow, tile["row"].asInt());
      maxRow = std::max(maxRow, tile["row"].asInt());
    }
    minRow = std::min(minRow, row);
    maxRow = std::max(maxRow, row);
    for (int r = minRow; r <= maxRow; ++r) {
      if (rows.find(r) == rows.end()) {
        LOG_DEBUG << "row " << r << " not found";
        return false;

      } else {
        LOG_DEBUG << "row " << r << " found";
      }
    }
  }
  return true;
}

std::vector<std::vector<Json::Value>>
getAffectedEquations(const std::vector<Json::Value> &state,
                     const std::vector<Json::Value> &current) {
  using Coord = std::pair<int, int>;
  std::map<Coord, Json::Value> board;
  auto addTiles = [&](const std::vector<Json::Value> &tiles) {
    for (auto &t : tiles) {
      int r = t["row"].asInt();
      int c = t["col"].asInt();
      board[{r, c}] = t;
    }
  };
  addTiles(state);
  addTiles(current);
  std::vector<std::vector<Json::Value>> affected;
  std::set<std::string> seen;

  for (auto &t : current) {
    int r = t["row"].asInt();
    int c = t["col"].asInt();

    std::vector<Json::Value> rowSeq;

    int cc = c;
    while (board.count({r, cc}))
      cc--;

    cc++;

    while (board.count({r, cc})) {
      rowSeq.push_back(board[{r, cc}]);
      cc++;
    }
    if (rowSeq.size() > 1) {
      std::string key = "R:" + std::to_string(r) + ":" +
                        std::to_string(rowSeq.front()["col"].asInt()) + "-" +
                        std::to_string(rowSeq.back()["col"].asInt());
      if (!seen.count(key)) {
        seen.insert(key);
        affected.push_back(rowSeq);
      }
    }
    std::vector<Json::Value> colSeq;
    int rr = r;
    while (board.count({rr, c})) {
      rr--;
    }
    rr++;
    while (board.count({rr, c})) {
      colSeq.push_back(board[{rr, c}]);
      rr++;
    }

    if (colSeq.size() > 1) {
      std::string key = "C:" + std::to_string(c) + ":" +
                        std::to_string(colSeq.front()["row"].asInt()) + "-" +
                        std::to_string(colSeq.back()["row"].asInt());
      if (!seen.count(key)) {
        seen.insert(key);
        affected.push_back(colSeq);
      }
    }
  }
  return affected;
}

int precedence(char op) {
  if (op == '+' || op == '-')
    return 1;
  if (op == '*' || op == '/')
    return 2;
}

int applyOp(int a, int b, char op) {
  switch (op) {
  case '+':
    return a + b;
  case '-':
    return a - b;
  case '*':
    return a * b;
  case '/':
    if (b == 0)
      throw std::runtime_error("Division by zero");
    if (a % b != 0)
      throw std::runtime_error("Not divisible");
    return a / b;
  }
  throw std::runtime_error("invalid operator");
}

int evaluateExpression(const std::string &expr) {
  std::stack<int> values;
  std::stack<char> ops;

  size_t i = 0;
  while (i < expr.size()) {
    if (isdigit(expr[i])) {
      int val = 0;
      while (i < expr.size() && isdigit(expr[i])) {
        val = val * 10 + (expr[i] - '0');
        i++;
      }
      values.push(val);
    } else if (expr[i] == '+' || expr[i] == '-' || expr[i] == '*' ||
               expr[i] == '/') {
      while (!ops.empty() && precedence(ops.top()) >= precedence(expr[i])) {
        int b = values.top();
        values.pop();
        int a = values.top();
        values.pop();
        char op = ops.top();
        ops.pop();
        values.push(applyOp(a, b, op));
      }
      ops.push(expr[i]);
      i++;
    } else {
      throw std::runtime_error("Invalid character in expression");
    }
  }
  while (!ops.empty()) {
    int b = values.top();
    values.pop();
    int a = values.top();
    values.pop();
    char op = ops.top();
    ops.pop();
    values.push(applyOp(a, b, op));
  }
  return values.top();
}

std::vector<std::string> splitEquation(const std::string &eq) {
  std::vector<std::string> parts;
  std::stringstream ss(eq);
  std::string token;
  while (std::getline(ss, token, '=')) {
    parts.push_back(token);
  }
  return parts;
}

auto checkEndGame = [](RoomState &room) {
  return (room.tileBag.empty() &&
          (room.player1Rack.empty() || room.player2Rack.empty())) ||
         room.passes == 6;
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
      room.currentTurn = (room.currentTurn == 1) ? 2 : 1;

    } else if (msgType == "pass") {
      reset(room, playerTurn);
      room.currentTurn = (room.currentTurn == 1) ? 2 : 1;
      room.passes++;
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
      auto newTiles = drawTiles(room.tileBag, 8 - playerRack.size());
      for (auto &tile : newTiles) {
        playerRack.push_back(tile);
      }
      Json::Value rackJson;
      rackJson["type"] = "rack";
      rackJson["rack"] = Json::Value(Json::arrayValue);
      for (auto &item : playerRack) {
        rackJson["rack"].append(item);
      }
      WebSocketConnectionPtr wsConnPtr =
          (playerTurn == 1) ? room.player1Conn : room.player2Conn;
      if (wsConnPtr) {
        wsConnPtr->send(
            Json::writeString(Json::StreamWriterBuilder(), rackJson));
      }
      room.currentTurn = (room.currentTurn == 1) ? 2 : 1;
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
      room.player2Conn->send(
          Json::writeString(Json::StreamWriterBuilder(), msg));
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
  if (params.find("isBot") != params.end() && params["isBot"] == "1") {
    room.isBot = true;
  } else {
    room.isBot = false;
  }
  Json::Value init;
  init["type"] = "init";
  init["turn"] = room.currentTurn;

  init["room name"] = s.chatRoomName_;
  if (!room.player1Conn) {
    room.tileBag = createTileBag();
    room.player1Conn = wsConnPtr;
    init["rack"] = Json::Value(Json::arrayValue);
    auto rack = drawTiles(room.tileBag, 8);
    room.player1Rack = rack;
    for (auto tile : rack) {
      init["rack"].append(tile);
    }
    init["sent"] = 1;
    if (room.isBot && !room.player2Conn) {
      room.player2Conn = nullptr; // No actual connection for bot
      auto rack = drawTiles(room.tileBag, 8);
      room.player2Rack = rack;
    }
  } else if (!room.isBot && !room.player2Conn) {
    room.player2Conn = wsConnPtr;
    init["rack"] = Json::Value(Json::arrayValue);
    auto rack = drawTiles(room.tileBag, 8);
    room.player2Rack = rack;
    for (auto tile : rack) {
      init["rack"].append(tile);
    }
    init["sent"] = 2;
  } else {
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
}
