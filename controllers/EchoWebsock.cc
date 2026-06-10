// EchoWebsock.cc
#include "EchoWebsock.h"
#include "../utils/Board.h"
#include "../utils/GameLogic.h"
#include "../utils/RoomState.h"
#include "../utils/Subscriber.h"
#include "../utils/ValidatorHelpers.h"
#include "LiveStatsController.h"
#include <algorithm>
#include <drogon/HttpTypes.h>
#include <drogon/PubSubService.h>
#include <drogon/WebSocketConnection.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Mapper.h>
#include <drogon/utils/coroutine.h>
#include <memory>
#include <random>
#include <stack>
#include <stdexcept>
#include <string>
#include <trantor/utils/Logger.h>
#include <type_traits>
#include <variant>
#include <vector>

using namespace drogon::orm;

namespace {
std::string serializeJsonMinified(const Json::Value &root) {
  thread_local static Json::StreamWriterBuilder writerBuilder;
  thread_local static bool initialized = []() {
    writerBuilder["commentStyle"] = "None";
    writerBuilder["indentation"] = "";
    return true;
  }();
  return Json::writeString(writerBuilder, root);
}
} // namespace

void EchoWebsock::handleNewMessage(const WebSocketConnectionPtr &wsConnPtr,
                                   std::string &&message,
                                   const WebSocketMessageType &type) {
  // write your application logic here
  if (type == WebSocketMessageType::Ping) {
    LOG_DEBUG << "recv a ping";
  } else if (type == WebSocketMessageType::Text) {
    auto &s = wsConnPtr->getContextRef<Subscriber>();
    Json::Value root;
    thread_local static Json::CharReaderBuilder readerBuilder;
    std::string errs;
    thread_local static std::unique_ptr<Json::CharReader> reader(
        readerBuilder.newCharReader());
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
      chatRooms_.publish(s.chatRoomName_, serializeJsonMinified(response));
      return;
    }
    if ((msgType == "placement" || msgType == "evaluate") &&
        playerTurn != room.currentTurn) {
      response["type"] = "error";
      response["message"] = "Not your turn";
      response["writer"] = playerTurn;
      response["turn"] = room.currentTurn;
      chatRooms_.publish(s.chatRoomName_, serializeJsonMinified(response));
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

      chatRooms_.publish(s.chatRoomName_, serializeJsonMinified(stateResponse));
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
        wsConnPtr->send(serializeJsonMinified(response));
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
          wsConnPtr->send(serializeJsonMinified(response));
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
      startTurnTimer(s.chatRoomName_);
      if (room.isBotGame && room.currentTurn == 2) {
        executeBotMove(s.chatRoomName_);
      }

    } else if (msgType == "pass") {
      reset(room, playerTurn);
      room.passes++;

      if (playerTurn == 2) {
        room.currentTurn = 1;
      } else {
        room.currentTurn = 2;
      }
      startTurnTimer(s.chatRoomName_);
      if (room.isBotGame && room.currentTurn == 2) {
        executeBotMove(s.chatRoomName_);
      }
    } else if (msgType == "evaluate") {
      if (room.state_.empty()) {
        if (!touchesCenter(room.current_)) {

          response["type"] = "error";
          response["message"] = "First move must cover the center (8, 8)";
          std::string jsonStr = serializeJsonMinified(response);
          chatRooms_.publish(s.chatRoomName_, jsonStr);

          return;
        }
      } else {
        if (!touchesExisting(room.state_, room.current_)) {
          response["type"] = "error";
          response["messsage"] = "Move must connect to existing tiles.";
          wsConnPtr->send(serializeJsonMinified(response));
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
          chatRooms_.publish(s.chatRoomName_,
                             serializeJsonMinified(errorResponse));

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
            chatRooms_.publish(s.chatRoomName_,
                               serializeJsonMinified(errorResponse));

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
      MoveRecord record;
      record.playerSide = playerTurn;
      record.tiles = room.current_;
      record.scoreGained = currentScore;
      room.moveHistory.push_back(record);

      room.current_.clear();
      room.passes = 0;
      std::vector<std::string> &playerRack =
          (playerTurn == 1) ? room.player1Rack : room.player2Rack;
      auto newTiles = drawTiles(room.tileBag, 7 - playerRack.size());
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

        room.player2Conn->send(serializeJsonMinified(rackJson));

      } else {
        room.player1Conn->send(serializeJsonMinified(rackJson));
        room.currentTurn = 2;
      }
      startTurnTimer(s.chatRoomName_);
      std::string jsonStr = serializeJsonMinified(response);
      chatRooms_.publish(s.chatRoomName_, jsonStr);

      if (room.isBotGame && room.currentTurn == 2) {
        executeBotMove(s.chatRoomName_);
      }

    } else if (msgType == "placement") {
      Json::Value payload = root["payload"];
      if (isOccupied(room.state_, room.current_, payload["row"].asInt(),
                     payload["col"].asInt())) {
        response["type"] = "error";
        response["message"] = "Cell already occupied";

        std::string jsonStr = serializeJsonMinified(response);
        chatRooms_.publish(s.chatRoomName_, jsonStr);
        return;
      }
      if (!isStraightLine(room.current_, payload["row"].asInt(),
                          payload["col"].asInt())) {
        response["type"] = "error";
        response["message"] = "Not in straight line.";
        std::string jsonStr = serializeJsonMinified(response);
        chatRooms_.publish(s.chatRoomName_, jsonStr);
        return;
      }
      if (!isContiguous(room.state_, room.current_, payload["row"].asInt(),
                        payload["col"].asInt())) {
        response["type"] = "error";
        response["message"] = "Not contiguous";
        std::string jsonStr = serializeJsonMinified(response);
        chatRooms_.publish(s.chatRoomName_, jsonStr);
        return;
      }
      std::string tileValue = payload["value"].asString();
      auto &rack = (playerTurn == 1) ? room.player1Rack : room.player2Rack;
      auto it = std::find(rack.begin(), rack.end(), tileValue);
      if (it == rack.end()) {
        response["type"] = "error";
        response["message"] = "Tile not in rack.";
        chatRooms_.publish(s.chatRoomName_, serializeJsonMinified(response));
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
      room.player1Conn->send(serializeJsonMinified(msg));
    } else {
      auto &rack = room.player2Rack;
      Json::Value msg;
      msg["type"] = "rack";
      msg["rack"] = Json::Value(Json::arrayValue);
      for (auto &item : rack) {
        msg["rack"].append(item);
      }
      room.player2Conn->send(serializeJsonMinified(msg));
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
    std::string jsonStr = serializeJsonMinified(response);
    chatRooms_.publish(s.chatRoomName_, jsonStr);
    auto win = checkEndGame(room);
    if (win) {

      Json::Value winResponse;
      winResponse["type"] = "game_over";
      int winner = 0;
      if (room.score1 > room.score2) {
        winner = 1;
      } else if (room.score2 > room.score1) {
        winner = 2;
      }

      auto db = drogon::app().getDbClient();

      if (room.challengeId != -1 && winner != 0) {
        drogon::async_run([db, winner, room]() -> drogon::Task<> {
          try {
            co_await db->execSqlCoro(
                "UPDATE challenges SET status='completed', winner=$1, "
                "completed_at = now() WHERE id=$2",
                winner == 1 ? room.player1Uid : room.player2Uid,
                room.challengeId);
            LOG_INFO << "Challenge updated";
          } catch (const DrogonDbException &e) {
            LOG_ERROR << e.base().what();
          }
          co_return;
        });
      }

      winResponse["winner"] = winner;
      winResponse["score1"] = room.score1;
      winResponse["score2"] = room.score2;

      chatRooms_.publish(s.chatRoomName_, serializeJsonMinified(winResponse));

      if (winner != 0) {
        auto winnerUid = (winner == 1) ? room.player1Uid : room.player2Uid;
        auto loserUid = (winner == 1) ? room.player2Uid : room.player1Uid;
        auto clientPtr = drogon::app().getDbClient();
        saveGameReview(room, winnerUid);
        applyGameRewards(winnerUid, loserUid, false);

      } else {
        // Draw case
        saveGameReview(room, "draw");
        auto db = drogon::app().getDbClient();

        if (room.challengeId != -1) {
          drogon::async_run([db, room]() -> drogon::Task<> {
            try {
              co_await db->execSqlCoro(
                  "UPDATE challenges SET status='completed', winner=NULL, "
                  "completed_at=now() WHERE id=$1",
                  room.challengeId);
              LOG_INFO << "Challenge updated";
            } catch (const DrogonDbException &e) {
              LOG_ERROR << e.base().what();
            }
            co_return;
          });
        }

        drogon::async_run([db, room]() -> drogon::Task<> {
          try {
            auto r = co_await db->execSqlCoro(
                "SELECT uid, elo FROM users WHERE uid=$1 OR uid=$2",
                room.player1Uid, room.player2Uid);

            if (r.size() == 2) {
              std::string uid1 = r[0]["uid"].as<std::string>();
              std::string uid2 = r[1]["uid"].as<std::string>();
              double elo1 = r[0]["elo"].as<double>();
              double elo2 = r[1]["elo"].as<double>();
              const double K = 32.0;

              double expected1 = 1.0 / (1.0 + pow(10.0, (elo2 - elo1) / 400.0));
              double expected2 = 1.0 / (1.0 + pow(10.0, (elo1 - elo2) / 400.0));

              double newElo1 = elo1 + K * (0.5 - expected1);
              double newElo2 = elo2 + K * (0.5 - expected2);

              co_await db->execSqlCoro(
                  "UPDATE users SET gamesplayed = gamesplayed + 1, draws = "
                  "draws + 1, elo=$1 WHERE uid=$2",
                  newElo1, uid1);

              co_await db->execSqlCoro(
                  "UPDATE users SET gamesplayed = gamesplayed + 1, draws = "
                  "draws + 1, elo=$1 WHERE uid=$2",
                  newElo2, uid2);

              co_await db->execSqlCoro(
                  "UPDATE stats SET current_win_streak = 0, best_elo = "
                  "GREATEST(best_elo, $2) WHERE uid = $1",
                  uid1, newElo1);
              LOG_INFO << "Updated p1 stats(draw): " << uid1;

              co_await db->execSqlCoro(
                  "UPDATE stats SET current_win_streak = 0, best_elo = "
                  "GREATEST(best_elo, $2) WHERE uid = $1",
                  uid2, newElo2);
              LOG_INFO << "Updated p2 stats(draw): " << uid2;
            }
          } catch (const DrogonDbException &e) {
            LOG_ERROR << "Failed to handle draw: " << e.base().what();
          }
          co_return;
        });
      }
    }
  }
}
// Framework-mandated void signature — cannot be a coroutine directly.
// We immediately hand off to the Task<> coroutine so the event loop is
// never blocked waiting on Postgres.
void EchoWebsock::handleNewConnection(const HttpRequestPtr &req,
                                      const WebSocketConnectionPtr &wsConnPtr) {
  drogon::async_run([req, wsConnPtr, this]() -> drogon::Task<> {
    co_await onNewConnectionAsync(req, wsConnPtr);
  });
}

// The real connection logic lives here as a coroutine.
// Every co_await suspends this function and returns control to the event loop
// while Postgres is busy, so other connections are processed normally.
drogon::Task<>
EchoWebsock::onNewConnectionAsync(HttpRequestPtr req,
                                  WebSocketConnectionPtr wsConnPtr) {
  LOG_DEBUG << "new websocket connection!";

  Subscriber s;
  s.chatRoomName_ = req->getParameter("room_name");
  const std::string uid = req->getParameter("uid");
  LiveStatsController::incrementActivePlayers();

  auto &room = rooms[s.chatRoomName_];
  room.isBotGame = req->getParameter("isBot") == "1";

  if (!room.isBotGame &&
      s.chatRoomName_.find("challenge") != std::string::npos) {
    try {
      room.challengeId = std::stoi(req->getParameter("room_name").substr(9));
    } catch (...) {
      room.challengeId = -1;
      LOG_WARN << "Invalid challenge room name: " << s.chatRoomName_;
    }
  } else {
    room.challengeId = -1;
  }

  Json::Value init;
  init["type"] = "init";
  init["turn"] = room.currentTurn;
  init["room name"] = s.chatRoomName_;

  auto db = drogon::app().getDbClient();

  // Helper lambda: fetches one player row, non-blocking.
  // co_await suspends HERE — event loop stays free during the DB round trip.
  auto fetchPlayer =
      [&](const std::string &playerUid) -> drogon::Task<drogon::orm::Result> {
    co_return co_await db->execSqlCoro(
        "SELECT name, photo, elo FROM users WHERE uid=$1", playerUid);
  };

  if (!room.player1Conn) {
    room.tileBag = createTileBag();
    room.player1Conn = wsConnPtr;
    room.player1Uid = uid;

    init["rack"] = Json::Value(Json::arrayValue);
    auto rack = drawTiles(room.tileBag, 7);
    room.player1Rack = rack;
    for (auto &tile : rack)
      init["rack"].append(tile);
    init["sent"] = 1;

    // ── Was execSqlSync (blocked event loop). Now suspends cleanly. ──
    auto result = co_await fetchPlayer(uid);
    if (result.empty()) {
      LOG_ERROR << "User not found: " << uid;
      wsConnPtr->forceClose();
      co_return;
    }
    init["name"] = result[0]["name"].as<std::string>();
    init["photo"] = result[0]["photo"].as<std::string>();
    init["elo"] = result[0]["elo"].as<double>();
    room.player2Conn = nullptr;

  } else if (room.player2Conn == nullptr) {
    room.player2Conn = wsConnPtr;
    room.player2Uid = uid;

    init["rack"] = Json::Value(Json::arrayValue);
    auto rack = drawTiles(room.tileBag, 7);
    room.player2Rack = rack;
    for (auto &tile : rack)
      init["rack"].append(tile);
    init["sent"] = 2;

    // ── Three separate execSqlSync calls collapsed into three co_awaits. ──
    auto r = co_await fetchPlayer(uid);
    auto r1 = co_await fetchPlayer(room.player1Uid);
    auto r2 = co_await fetchPlayer(room.player2Uid);

    if (r.empty() || r1.empty() || r2.empty()) {
      LOG_ERROR << "One or more players not found in DB";
      wsConnPtr->forceClose();
      co_return;
    }

    init["name"] = r[0]["name"].as<std::string>();
    init["photo"] = r[0]["photo"].as<std::string>();
    init["elo"] = r[0]["elo"].as<double>();

    Json::Value opp1, opp2;
    opp1["type"] = "opponent_info";
    opp1["name"] = r1[0]["name"].as<std::string>();
    opp1["photo"] = r1[0]["photo"].as<std::string>();
    opp1["elo"] = r1[0]["elo"].as<double>();

    opp2["type"] = "opponent_info";
    opp2["name"] = r2[0]["name"].as<std::string>();
    opp2["photo"] = r2[0]["photo"].as<std::string>();
    opp2["elo"] = r2[0]["elo"].as<double>();

    room.player1Conn->send(serializeJsonMinified(opp2));
    room.player2Conn->send(serializeJsonMinified(opp1));

    room.currentTurn = 1;
    startTurnTimer(s.chatRoomName_);

  } else {
    init["error"] = "2 Players already connected";
    wsConnPtr->send(serializeJsonMinified(init));
    s.id_ = chatRooms_.subscribe(
        s.chatRoomName_,
        [wsConnPtr](const std::string &, const std::string &msg) {
          wsConnPtr->send(msg);
        });
    wsConnPtr->setContext(std::make_shared<Subscriber>(std::move(s)));
    wsConnPtr->forceClose();
    co_return;
  }

  s.id_ = chatRooms_.subscribe(
      s.chatRoomName_,
      [wsConnPtr](const std::string &, const std::string &msg) {
        wsConnPtr->send(msg);
      });
  wsConnPtr->setContext(std::make_shared<Subscriber>(std::move(s)));
  wsConnPtr->send(serializeJsonMinified(init));
}
void EchoWebsock::handleConnectionClosed(
    const WebSocketConnectionPtr &wsConnPtr) {
  // write your application logic here
  LOG_DEBUG << "websocket closed!";
  auto &s = wsConnPtr->getContextRef<Subscriber>();
  chatRooms_.unsubscribe(s.chatRoomName_, s.id_);
  LiveStatsController::decrementActivePlayers();

  // Clear the room connection to allow new players to join
  auto &room = rooms[s.chatRoomName_];
  int winnerSide = 0;
  // Check if this was player1 or player2
  if (room.player1Conn == wsConnPtr) {
    room.player1Conn = nullptr;
    winnerSide = 2;
  } else if (room.player2Conn == wsConnPtr) {
    room.player2Conn = nullptr;
    winnerSide = 1;
  }

  if (winnerSide != 0 && !room.player1Uid.empty() && !room.player2Uid.empty()) {
    handleForfeit(s.chatRoomName_, winnerSide, "opponent disconnected");
  }
}

void EchoWebsock::startTurnTimer(const std::string &roomName) {
  auto &room = rooms[roomName];
  auto loop = drogon::app().getLoop();
  if (room.activeTimerId != 0) {
    loop->invalidateTimer(room.activeTimerId);
  }
  room.activeTimerId = loop->runAfter(room.TURN_TIME_LIMIT, [this, roomName]() {
    auto &r = rooms[roomName];
    int current = r.currentTurn;

    int &timeoutCount = (current == 1) ? r.p1Timeouts : r.p2Timeouts;
    timeoutCount++;

    if (timeoutCount >= 2) {
      handleForfeit(roomName, (current == 1) ? 2 : 1,
                    "two timeouts - auto forfeit");
    } else {
      r.currentTurn = current == 1 ? 2 : 1;
      broadcastState(roomName);
      startTurnTimer(roomName);
      if (r.isBotGame && r.currentTurn == 2) {
        LOG_INFO << "Bot's turn after timeout. Executing bot move for "
                 << roomName;
        executeBotMove(roomName);
      }
    }
  });
}

void EchoWebsock::broadcastState(const std::string &roomName) {
  auto &room = rooms[roomName];
  Json::Value response;

  response["type"] = "state";
  response["turn"] = room.currentTurn;
  response["player1Score"] = room.score1;
  response["player2Score"] = room.score2;
  response["p1Timeouts"] = room.p1Timeouts;
  response["p2Timeouts"] = room.p2Timeouts;

  Json::Value tiles(Json::arrayValue);
  for (const auto &tile : room.state_) {
    tiles.append(tile);
  }
  response["tiles"] = tiles;

  std::string jsonStr = serializeJsonMinified(response);
  chatRooms_.publish(roomName, jsonStr);
}

void EchoWebsock::handleForfeit(const std::string &roomName, int winnerSide,
                                const std::string &reason) {
  auto &room = rooms[roomName];
  stopTimer(roomName);
  std::string winnerUid = (winnerSide == 1) ? room.player1Uid : room.player2Uid;
  std::string loserUid = (winnerSide == 1) ? room.player2Uid : room.player1Uid;
  int cId = room.challengeId;
  saveGameReview(room, winnerUid);
  Json::Value ovr;
  ovr["type"] = "game_over";
  ovr["winner"] = winnerSide;
  ovr["reason"] = reason;
  chatRooms_.publish(roomName, serializeJsonMinified(ovr));

  if (room.isBotGame) {
    LOG_INFO << "Bot game - db update ignored.";
    return;
  }
  auto db = drogon::app().getDbClient();
  LOG_INFO << "Forfeit triggered - Room: " << roomName
           << " | ChallengeID: " << cId << " | Winner: " << winnerUid;

  drogon::async_run([this, db, roomName, cId, winnerUid, loserUid,
                     room]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "UPDATE challenges SET status='forfeit' WHERE id=$1",
          room.challengeId);
      LOG_INFO << "Forfeit DB Callback - Rows affected: " << r.affectedRows();
      this->applyGameRewards(winnerUid, loserUid, true);
    } catch (const DrogonDbException &e) {
      LOG_ERROR << "Failed to update challenge for forfeit: "
                << e.base().what();
    }
    co_return;
  });
}

void EchoWebsock::stopTimer(const std::string &roomName) {
  auto it = rooms.find(roomName);
  if (it != rooms.end()) {
    auto &room = it->second;
    if (room.activeTimerId != 0) {
      drogon::app().getLoop()->invalidateTimer(room.activeTimerId);
      room.activeTimerId = 0;
      LOG_DEBUG << "Timer stopped for room: " << roomName;
    }
  }
}

void EchoWebsock::applyGameRewards(const std::string &winnerUid,
                                   const std::string &loserUid,
                                   bool isForfeit) {
  if (winnerUid == "BOT_AI" || loserUid == "BOT_AI") {
    LOG_INFO << "Bot game - skipping Elo and stats update.";
    return;
  }

  auto db = drogon::app().getDbClient();

  drogon::async_run([db, winnerUid, loserUid, isForfeit]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT uid, elo FROM users WHERE uid=$1 OR uid=$2", winnerUid,
          loserUid);

      if (r.size() != 2)
        co_return;

      double wElo = 1000.0, lElo = 1000.0;
      for (auto const &row : r) {
        if (row["uid"].as<std::string>() == winnerUid)
          wElo = row["elo"].as<double>();
        else
          lElo = row["elo"].as<double>();
      }

      const double K = 32.0;
      double expectedWinner = 1.0 / (1.0 + pow(10.0, (lElo - wElo) / 400.0));
      double eloChange = K * (1.0 - expectedWinner);

      co_await db->execSqlCoro(
          "UPDATE users SET elo = elo + $1, wins = wins + 1, gamesplayed = "
          "gamesplayed + 1 WHERE uid = $2",
          eloChange, winnerUid);

      co_await db->execSqlCoro(
          "UPDATE users SET elo = GREATEST(0, elo - $1), losses = losses + 1, "
          "gamesplayed = gamesplayed + 1 WHERE uid = $2",
          eloChange, loserUid);

      co_await db->execSqlCoro(
          "UPDATE stats SET current_win_streak = current_win_streak + 1, "
          "best_win_streak = GREATEST(best_win_streak, current_win_streak + "
          "1), best_elo = GREATEST(best_elo, (SELECT elo FROM users WHERE "
          "uid=$1)) WHERE uid = $1",
          winnerUid);

      co_await db->execSqlCoro(
          "UPDATE stats SET current_win_streak = 0 WHERE uid = $1", loserUid);

      LOG_INFO << "Consistently updated stats for "
               << (isForfeit ? "forfeit" : "normal") << " win.";
    } catch (const DrogonDbException &e) {
      LOG_ERROR << "Failed to apply game rewards: " << e.base().what();
    }
    co_return;
  });
}

void EchoWebsock::executeBotMove(const std::string &roomName) {
  auto it = rooms.find(roomName);
  if (it == rooms.end())
    return;
  auto &room = it->second;

  // Snapshot the state the bot needs — immutable copies so the bot thread
  // never touches live room state while the IO thread may also be reading it.
  auto boardSnapshot = room.state_;
  auto rackSnapshot = room.player2Rack;

  // Submit CPU-bound work to the dedicated compute pool.
  // The IO event loop returns immediately — other WebSocket messages continue.
  try {
    botPool_.submit([this, roomName, board = std::move(boardSnapshot),
                     rack = std::move(rackSnapshot)]() mutable {
      // ── Running on bot-compute thread, NOT the IO thread ──
      BotMove botMove = GameLogic::findBestMove(board, rack);

      // Post the result back to the IO thread — the only thread allowed
      // to mutate room state. queueInLoop() is the thread-safe handoff.
      drogon::app().getLoop()->queueInLoop(
          [this, roomName, botMove = std::move(botMove)]() mutable {
            // ── Back on IO thread — safe to access rooms map ──
            auto roomIt = rooms.find(roomName);
            if (roomIt == rooms.end())
              return; // room closed while bot was thinking
            auto &r = roomIt->second;

            if (botMove.isValid) {
              for (const auto &tile : botMove.placedTiles)
                r.state_.push_back(tile);
              r.score2 += botMove.score;
              r.passes = 0;

              for (const auto &val : botMove.usedValues) {
                auto rackIt = std::ranges::find(r.player2Rack, val);
                if (rackIt != r.player2Rack.end())
                  r.player2Rack.erase(rackIt);
              }
              auto newTiles = drawTiles(r.tileBag, RoomState::RACK_SIZE -
                                                       r.player2Rack.size());
              r.player2Rack.insert(r.player2Rack.end(), newTiles.begin(),
                                   newTiles.end());
            } else {
              r.passes++;
              LOG_INFO << "Bot passed (no valid move) in room: " << roomName;
            }

            r.currentTurn = 1;
            broadcastState(roomName);
            startTurnTimer(roomName);
          });
    });
  } catch (const std::exception &e) {
    LOG_ERROR << "Failed to submit bot move task: " << e.what();
  }
}

void EchoWebsock::saveGameReview(const RoomState &room,
                                 const std::string &winnerUid) {
  Json::Value movesJson(Json::arrayValue);
  for (const auto &m : room.moveHistory) {
    Json::Value move;
    move["side"] = m.playerSide;
    move["score"] = m.scoreGained;
    Json::Value tiles(Json::arrayValue);
    for (const auto &t : m.tiles) {
      tiles.append(t);
    }
    move["tiles"] = tiles;
    movesJson.append(move);
  }

  auto db = drogon::app().getDbClient();

  drogon::async_run([db, room, winnerUid, movesJson]() -> drogon::Task<> {
    try {
      co_await db->execSqlCoro(
          "INSERT INTO game_history (room_id, player1_uid, player2_uid, "
          "winner_uid, final_score_p1, final_score_p2, moves) VALUES ($1, $2, "
          "$3, $4, $5, $6, $7)",
          std::to_string(room.challengeId), room.player1Uid, room.player2Uid,
          winnerUid, room.score1, room.score2,
          serializeJsonMinified(movesJson));
      LOG_INFO << "Game review successfully saved to game_history.";
    } catch (const drogon::orm::DrogonDbException &e) {
      LOG_ERROR << "Failed to save game review: " << e.base().what();
    }
    co_return;
  });
}