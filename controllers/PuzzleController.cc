#include "PuzzleController.h"
#include "../utils/Board.h"
#include "../utils/GameLogic.h"
#include "../utils/ValidatorHelpers.h"
#include <drogon/drogon.h>
#include <drogon/orm/Mapper.h>
#include <json/json.h>

using namespace drogon;
using namespace drogon::orm;

void PuzzleController::validateMove(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // 1️⃣ Get UID from request attributes
  auto uidAttr = req->attributes()->get<std::string>("uid");
  if (uidAttr.empty()) {
    auto resp = HttpResponse::newHttpJsonResponse(
        Json::Value("Missing UID in request"));
    resp->setStatusCode(k401Unauthorized);
    callback(resp);
    return;
  }
  std::string userId = uidAttr;

  // 2️⃣ Parse JSON body
  Json::Value root;
  Json::CharReaderBuilder builder;
  std::string errs;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  std::string body{req->getBody()};
  if (!reader->parse(body.c_str(), body.c_str() + body.size(), &root, &errs)) {
    auto resp =
        HttpResponse::newHttpJsonResponse(Json::Value("Invalid JSON: " + errs));
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  int puzzleId = root["puzzle_id"].asInt();
  Json::Value moveTiles = root["placed_tiles"]; // array of {row, col, value}

  auto db = app().getDbClient();

  // 3️⃣ Fetch puzzle board and objective
  db->execSqlAsync(
      "SELECT board_state, objective FROM puzzles WHERE puzzle_id=$1",
      [callback, moveTiles, userId, puzzleId](const Result &r) {
        if (r.empty()) {
          auto resp = HttpResponse::newHttpJsonResponse(Json::Value(
              "Puzzle not found for puzzle_id=" + std::to_string(puzzleId)));
          resp->setStatusCode(k404NotFound);
          callback(resp);
          return;
        }

        // Parse board JSON
        Json::Value boardJson;
        std::string boardStr = r[0]["board_state"].as<std::string>();
        Json::CharReaderBuilder builder;
        std::string errs;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        reader->parse(boardStr.c_str(), boardStr.c_str() + boardStr.size(),
                      &boardJson, &errs);

        std::string objective = r[0]["objective"].as<std::string>();

        // Convert JSON arrays to std::vector<Json::Value> for helpers
        std::vector<Json::Value> boardVec(boardJson.begin(), boardJson.end());
        std::vector<Json::Value> moveTilesVec;

        // --- VALIDATION LOGIC ---

        // 1️⃣ Check if cells are already occupied
        for (auto &tile : moveTilesVec) {
          int row = tile["row"].asInt();
          int col = tile["col"].asInt();
          std::string value = tile["value"].asString();

          if (isOccupied(moveTilesVec, boardVec, row, col)) {
            std::string msg = "Cell already occupied at row " +
                              std::to_string(row) + ", col " +
                              std::to_string(col) + " for tile '" + value + "'";
            auto resp = HttpResponse::newHttpJsonResponse(Json::Value(msg));
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
          }
          moveTilesVec.push_back(tile);
        }

        // 2️⃣ Check straight line
        if (!isStraightLine(moveTilesVec, -1, -1)) {
          std::string msg = "Tiles are not in a straight line. Tiles placed:";
          for (auto &t : moveTilesVec)
            msg += " (" + std::to_string(t["row"].asInt()) + "," +
                   std::to_string(t["col"].asInt()) + ")";
          auto resp = HttpResponse::newHttpJsonResponse(Json::Value(msg));
          resp->setStatusCode(k400BadRequest);
          callback(resp);
          return;
        }

        // 3️⃣ Check contiguity
        if (!isContiguous(boardVec, moveTilesVec, -1, -1)) {
          std::string msg = "Tiles are not contiguous. Tiles placed:";
          for (auto &t : moveTilesVec)
            msg += " (" + std::to_string(t["row"].asInt()) + "," +
                   std::to_string(t["col"].asInt()) + ")";
          auto resp = HttpResponse::newHttpJsonResponse(Json::Value(msg));
          resp->setStatusCode(k400BadRequest);
          callback(resp);
          return;
        }

        // 4️⃣ Validate affected equations
        auto affected = getAffectedEquations(boardVec, moveTilesVec);
        for (auto &seq : affected) {
          std::string expr;
          for (auto &t : seq)
            expr += t["value"].asString();

          auto parts = splitEquation(expr);
          if (parts.size() < 2) {
            auto resp = HttpResponse::newHttpJsonResponse(
                Json::Value("Invalid equation (must contain '='): " + expr));
            resp->setStatusCode(k400BadRequest);
            callback(resp);
            return;
          }

          auto val = evaluateExpression(parts[0]);
          for (auto &part : parts) {
            if (evaluateExpression(part) != val) {
              auto resp = HttpResponse::newHttpJsonResponse(Json::Value(
                  "Equation does not hold: " + expr + ", part: " + part));
              resp->setStatusCode(k400BadRequest);
              callback(resp);
              return;
            }
          }
        }

        // ✅ Move is valid
        auto resp =
            HttpResponse::newHttpJsonResponse(Json::Value("Move valid"));
        resp->setStatusCode(k200OK);
        callback(resp);

        auto db = app().getDbClient();
        db->execSqlAsync(
            "INSERT INTO puzzle_progress(user_id, puzzle_id, solved, "
            "last_updated) "
            "VALUES ($1, $2, TRUE, NOW()) "
            "ON CONFLICT(user_id, puzzle_id) "
            "DO UPDATE SET solved = TRUE, last_updated = NOW()",
            [userId, puzzleId](const Result &r) {
              // optional: log success
              LOG_DEBUG << "Puzzle progress updated for user=" << userId
                        << ", puzzle=" << puzzleId;
            },
            [userId, puzzleId](const DrogonDbException &e) {
              LOG_ERROR << "Failed to update puzzle_progress for user="
                        << userId << ", puzzle=" << puzzleId << ": "
                        << e.base().what();
            },
            userId, puzzleId);
      },
      [callback](const DrogonDbException &e) {
        auto resp = HttpResponse::newHttpJsonResponse(
            Json::Value("Database error: " + std::string(e.base().what())));
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
      },
      puzzleId);
}

void PuzzleController::getPuzzle(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback, int puzzleId) {
  auto db = app().getDbClient();

  db->execSqlAsync(
      "SELECT puzzle_id, board_state, rack, objective, difficulty "
      "FROM puzzles WHERE puzzle_id=$1",
      [callback](const Result &r) {
        if (r.empty()) {
          auto resp = HttpResponse::newHttpJsonResponse(
              Json::Value("Puzzle not found"));
          resp->setStatusCode(k404NotFound);
          callback(resp);
          return;
        }

        Json::Value res;
        Json::Value boardJson;
        Json::Value rackJson;

        // Parse stored JSON strings
        Json::CharReaderBuilder builder;
        std::string errs;

        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        std::string boardStr = r[0]["board_state"].as<std::string>();
        reader->parse(boardStr.c_str(), boardStr.c_str() + boardStr.size(),
                      &boardJson, &errs);

        reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
        std::string rackStr = r[0]["rack"].as<std::string>();
        reader->parse(rackStr.c_str(), rackStr.c_str() + rackStr.size(),
                      &rackJson, &errs);

        res["puzzle_id"] = r[0]["puzzle_id"].as<int>();
        res["difficulty"] = r[0]["difficulty"].as<std::string>();
        res["objective"] = r[0]["objective"].as<std::string>();
        res["board"] = boardJson; // Already formatted as [{"row":...}]
        res["rack"] = rackJson;   // ["1","2","+"]

        auto resp = HttpResponse::newHttpJsonResponse(res);
        resp->setStatusCode(k200OK);
        callback(resp);
      },
      [callback](const DrogonDbException &e) {
        Json::Value err;
        err["error"] = e.base().what();
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
      },
      puzzleId);
}

void PuzzleController::listPuzzles(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  // 1. Extract UID from attributes (set by FirebaseAuthFilter)
  auto uidAttr = req->attributes()->get<std::string>("uid");
  if (uidAttr.empty()) {
    Json::Value err;
    err["error"] = "Missing UID";
    callback(HttpResponse::newHttpJsonResponse(err));
    return;
  }
  std::string userId = uidAttr;

  // 2. DB client
  auto db = app().getDbClient();

  // 3. Query puzzles + user progress in one SQL (LEFT JOIN)
  db->execSqlAsync(
      "SELECT p.puzzle_id, p.difficulty, p.objective, "
      "       COALESCE(pp.solved, FALSE) AS solved "
      "FROM puzzles p "
      "LEFT JOIN puzzle_progress pp "
      "  ON pp.puzzle_id = p.puzzle_id AND pp.user_id = $1 "
      "ORDER BY p.puzzle_id ASC",
      [callback](const Result &r) {
        Json::Value list(Json::arrayValue);

        for (auto const &row : r) {
          Json::Value item;
          item["puzzle_id"] = row["puzzle_id"].as<int>();
          item["difficulty"] = row["difficulty"].as<std::string>();
          item["objective"] = row["objective"].as<std::string>();
          item["solved"] = row["solved"].as<bool>();

          list.append(item);
        }

        auto resp = HttpResponse::newHttpJsonResponse(list);
        resp->setStatusCode(k200OK);
        callback(resp);
      },
      [callback](const DrogonDbException &e) {
        Json::Value err;
        err["error"] = e.base().what();
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k500InternalServerError);
        callback(resp);
      },
      userId // SQL parameter $1
  );
}
