#include "PuzzleController.h"
#include "../utils/Board.h"
#include "../utils/GameLogic.h"
#include "../utils/ValidatorHelpers.h"
#include <chrono>
#include <drogon/drogon.h>
#include <drogon/orm/Mapper.h>
#include <drogon/utils/coroutine.h>
#include <iomanip>
#include <json/json.h>
#include <memory>
#include <sstream>

using namespace drogon;
using namespace drogon::orm;

// ============================================================================
// Helper Methods
// ============================================================================

Json::Value PuzzleController::createErrorResponse(const std::string &message,
                                                  const std::string &code) {
  Json::Value err;
  err["error"] = true;
  err["message"] = message;
  err["code"] = code;
  return err;
}

Json::Value PuzzleController::createSuccessResponse(const Json::Value &data) {
  Json::Value resp;
  resp["error"] = false;
  resp["data"] = data;
  return resp;
}

// ============================================================================
// GET /puzzles - List puzzles with filtering/sorting/pagination
// ============================================================================

void PuzzleController::listPuzzles(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto uidAttr = req->attributes()->get<std::string>("uid");
  if (uidAttr.empty()) {
    auto resp =
        HttpResponse::newHttpJsonResponse(PuzzleController::createErrorResponse(
            "Not authenticated", "UNAUTHORIZED"));
    resp->setStatusCode(k401Unauthorized);
    callback(resp);
    return;
  }
  std::string userId = uidAttr;

  std::string difficulty = req->getParameter("difficulty");
  std::string sortBy = req->getParameter("sort");
  std::string search = req->getParameter("search");
  int limit = 50;
  int offset = 0;

  if (!req->getParameter("limit").empty()) {
    limit = std::min(100, std::stoi(req->getParameter("limit")));
  }
  if (!req->getParameter("offset").empty()) {
    offset = std::stoi(req->getParameter("offset"));
  }

  std::string whereClause = "";

  if (!difficulty.empty() && difficulty != "all") {
    whereClause += " AND p.difficulty = '" + difficulty + "'";
  }
  if (!search.empty()) {
    whereClause += " AND (p.objective ILIKE '%" + search +
                   "%' OR p.puzzle_id::text ILIKE '%" + search + "%')";
  }

  std::string orderClause = " ORDER BY p.puzzle_id ASC";
  if (sortBy == "recent") {
    orderClause = " ORDER BY p.created_at DESC";
  } else if (sortBy == "oldest") {
    orderClause = " ORDER BY p.created_at ASC";
  } else if (sortBy == "completed") {
    orderClause = " ORDER BY pp.solved DESC, p.puzzle_id ASC";
  } else if (sortBy == "pending") {
    orderClause = " ORDER BY CASE WHEN pp.solved IS FALSE THEN 0 ELSE 1 END, "
                  "p.puzzle_id ASC";
  }

  std::string countQuery =
      "SELECT COUNT(*) as total FROM puzzles p WHERE 1=1 " + whereClause;

  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, userId, limit, offset, whereClause, orderClause,
                     countQuery, cb]() -> drogon::Task<> {
    try {
      auto countResult = co_await db->execSqlCoro(countQuery);
      int total = 0;
      if (!countResult.empty()) {
        total = countResult[0]["total"].as<int>();
      }

      std::string query =
          "SELECT p.puzzle_id, p.difficulty, p.objective, p.created_at, "
          "COALESCE(pp.solved, FALSE) AS solved, pp.best_time FROM puzzles p "
          "LEFT JOIN puzzle_progress pp ON pp.puzzle_id = p.puzzle_id AND "
          "pp.user_id = $1 WHERE 1=1 " +
          whereClause + orderClause + " LIMIT " + std::to_string(limit) +
          " OFFSET " + std::to_string(offset);

      auto r = co_await db->execSqlCoro(query, userId);

      Json::Value dataArray(Json::arrayValue);
      for (auto const &row : r) {
        Json::Value item;
        item["puzzle_id"] = row["puzzle_id"].as<int>();
        item["difficulty"] = row["difficulty"].as<std::string>();
        item["objective"] = row["objective"].as<std::string>();
        item["solved"] = row["solved"].as<bool>();
        item["created_at"] = row["created_at"].as<std::string>();
        if (!row["best_time"].isNull()) {
          item["best_time"] = row["best_time"].as<int>();
        } else {
          item["best_time"] = Json::nullValue;
        }
        dataArray.append(item);
      }

      Json::Value resp;
      resp["data"] = dataArray;
      resp["total"] = total;
      resp["limit"] = limit;
      resp["offset"] = offset;

      auto httpResp = HttpResponse::newHttpJsonResponse(resp);
      httpResp->setStatusCode(k200OK);
      (*cb)(httpResp);
    } catch (const DrogonDbException &e) {
      auto resp = HttpResponse::newHttpJsonResponse(
          PuzzleController::createErrorResponse(std::string(e.base().what()),
                                                "SERVER_ERROR"));
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ============================================================================
// GET /puzzle/:puzzleId - Get single puzzle with board and rack
// ============================================================================

void PuzzleController::getPuzzle(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback, int puzzleId) {
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, puzzleId, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          R"(SELECT puzzle_id, board_state, rack, objective, difficulty, created_at FROM puzzles WHERE puzzle_id=$1)",
          puzzleId);

      if (r.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(
            PuzzleController::createErrorResponse("Puzzle not found",
                                                  "NOT_FOUND"));
        resp->setStatusCode(k404NotFound);
        (*cb)(resp);
        co_return;
      }

      Json::Value boardJson, rackJson;
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

      Json::Value data;
      data["puzzle_id"] = r[0]["puzzle_id"].as<int>();
      data["difficulty"] = r[0]["difficulty"].as<std::string>();
      data["objective"] = r[0]["objective"].as<std::string>();
      data["board"] = boardJson;
      data["rack"] = rackJson;
      data["created_at"] = r[0]["created_at"].as<std::string>();

      auto resp = HttpResponse::newHttpJsonResponse(
          PuzzleController::createSuccessResponse(data));
      resp->setStatusCode(k200OK);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      auto resp = HttpResponse::newHttpJsonResponse(
          PuzzleController::createErrorResponse(std::string(e.base().what()),
                                                "SERVER_ERROR"));
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ============================================================================
// POST /puzzle/validateMove - Validate a move and return score
// ============================================================================

void PuzzleController::validateMove(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto uidAttr = req->attributes()->get<std::string>("uid");
  if (uidAttr.empty()) {
    auto resp =
        HttpResponse::newHttpJsonResponse(PuzzleController::createErrorResponse(
            "Not authenticated", "UNAUTHORIZED"));
    resp->setStatusCode(k401Unauthorized);
    callback(resp);
    return;
  }
  std::string userId = uidAttr;

  Json::Value root;
  Json::CharReaderBuilder builder;
  std::string errs;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  std::string body{req->getBody()};
  if (!reader->parse(body.c_str(), body.c_str() + body.size(), &root, &errs)) {
    auto resp =
        HttpResponse::newHttpJsonResponse(PuzzleController::createErrorResponse(
            "Invalid JSON: " + errs, "INVALID_DATA"));
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  if (!root.isMember("puzzle_id") || !root.isMember("placed_tiles")) {
    auto resp =
        HttpResponse::newHttpJsonResponse(PuzzleController::createErrorResponse(
            "Missing puzzle_id or placed_tiles", "INVALID_DATA"));
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  int puzzleId = root["puzzle_id"].asInt();
  Json::Value moveTiles = root["placed_tiles"];

  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, cb, userId, puzzleId, moveTiles]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT board_state, objective FROM puzzles WHERE puzzle_id=$1",
          puzzleId);

      if (r.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(
            PuzzleController::createErrorResponse("Puzzle not found",
                                                  "NOT_FOUND"));
        resp->setStatusCode(k404NotFound);
        (*cb)(resp);
        co_return;
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
      std::vector<Json::Value> boardVec(boardJson.begin(), boardJson.end());
      std::vector<Json::Value> moveTilesVec;

      // --- VALIDATION LOGIC ---

      // 1. Check if cells are already occupied
      for (auto &tile : moveTiles) {
        int row = tile["row"].asInt();
        int col = tile["col"].asInt();
        std::string value = tile["value"].asString();

        if (isOccupied(moveTilesVec, boardVec, row, col)) {
          std::string msg = "Cell already occupied at row " +
                            std::to_string(row) + ", col " +
                            std::to_string(col) + ".";
          auto resp = HttpResponse::newHttpJsonResponse(
              PuzzleController::createErrorResponse(msg, "VALIDATION_FAILED"));
          resp->setStatusCode(k400BadRequest);
          (*cb)(resp);
          co_return;
        }
        moveTilesVec.push_back(tile);
      }

      // 2. Check straight line
      auto straightLineValidation = [](const std::vector<Json::Value> &tiles) {
        if (tiles.empty())
          return true;
        bool horizontal =
            std::all_of(tiles.begin(), tiles.end(), [&](const auto &tile) {
              return tile["row"].asInt() == tiles[0]["row"].asInt();
            });
        if (horizontal)
          return true;
        bool vertical =
            std::all_of(tiles.begin(), tiles.end(), [&](const auto &tile) {
              return tile["col"].asInt() == tiles[0]["col"].asInt();
            });
        return vertical;
      };

      if (!straightLineValidation(moveTilesVec)) {
        auto resp = HttpResponse::newHttpJsonResponse(
            PuzzleController::createErrorResponse(
                "Tiles must be in a straight line", "VALIDATION_FAILED"));
        resp->setStatusCode(k400BadRequest);
        (*cb)(resp);
        co_return;
      }

      auto touchesCenter = [](const std::vector<Json::Value> &tiles) {
        return std::any_of(tiles.begin(), tiles.end(), [](const auto &t) {
          return t["row"].asInt() == 8 && t["col"].asInt() == 8;
        });
      };

      auto touchesExisting = [](const std::vector<Json::Value> &board,
                                const std::vector<Json::Value> &tiles) {
        for (const auto &t : tiles) {
          int row = t["row"].asInt();
          int col = t["col"].asInt();
          std::vector<std::pair<int, int>> directions = {
              {0, 1}, {1, 0}, {0, -1}, {-1, 0}};
          for (auto [dr, dc] : directions) {
            int adjRow = row + dr;
            int adjCol = col + dc;
            for (const auto &b : board) {
              if (b["row"].asInt() == adjRow && b["col"].asInt() == adjCol) {
                return true;
              }
            }
          }
        }
        return false;
      };

      // 3. Check center/touching
      if (boardVec.empty()) {
        if (!touchesCenter(moveTilesVec)) {
          auto resp = HttpResponse::newHttpJsonResponse(
              PuzzleController::createErrorResponse(
                  "First move must cover center cell (8,8)",
                  "VALIDATION_FAILED"));
          resp->setStatusCode(k400BadRequest);
          (*cb)(resp);
          co_return;
        }
      } else if (!touchesExisting(boardVec, moveTilesVec)) {
        auto resp = HttpResponse::newHttpJsonResponse(
            PuzzleController::createErrorResponse(
                "Placed tiles must touch existing tiles", "VALIDATION_FAILED"));
        resp->setStatusCode(k400BadRequest);
        (*cb)(resp);
        co_return;
      }

      // 4. Check contiguity
      auto contiguousValidation = [](const std::vector<Json::Value> &board,
                                     const std::vector<Json::Value> &tiles) {
        if (tiles.empty())
          return true;
        bool sameRow =
            std::all_of(tiles.begin(), tiles.end(), [&](const auto &tile) {
              return tile["row"].asInt() == tiles[0]["row"].asInt();
            });
        bool sameCol =
            std::all_of(tiles.begin(), tiles.end(), [&](const auto &tile) {
              return tile["col"].asInt() == tiles[0]["col"].asInt();
            });
        if (!sameRow && !sameCol)
          return false;

        if (sameRow) {
          std::set<int> cols;
          for (const auto &tile : tiles)
            cols.insert(tile["col"].asInt());
          for (const auto &tile : board) {
            if (tile["row"].asInt() == tiles[0]["row"].asInt())
              cols.insert(tile["col"].asInt());
          }
          int minCol = tiles[0]["col"].asInt(),
              maxCol = tiles[0]["col"].asInt();
          for (const auto &tile : tiles) {
            minCol = std::min(minCol, tile["col"].asInt());
            maxCol = std::max(maxCol, tile["col"].asInt());
          }
          for (int c = minCol; c <= maxCol; ++c)
            if (cols.find(c) == cols.end())
              return false;
        } else if (sameCol) {
          std::set<int> rows;
          for (const auto &tile : tiles)
            rows.insert(tile["row"].asInt());
          for (const auto &tile : board) {
            if (tile["col"].asInt() == tiles[0]["col"].asInt())
              rows.insert(tile["row"].asInt());
          }
          int minRow = tiles[0]["row"].asInt(),
              maxRow = tiles[0]["row"].asInt();
          for (const auto &tile : tiles) {
            minRow = std::min(minRow, tile["row"].asInt());
            maxRow = std::max(maxRow, tile["row"].asInt());
          }
          for (int r = minRow; r <= maxRow; ++r)
            if (rows.find(r) == rows.end())
              return false;
        }
        return true;
      };

      if (!contiguousValidation(boardVec, moveTilesVec)) {
        auto resp = HttpResponse::newHttpJsonResponse(
            PuzzleController::createErrorResponse("Tiles are not contiguous",
                                                  "VALIDATION_FAILED"));
        resp->setStatusCode(k400BadRequest);
        (*cb)(resp);
        co_return;
      }

      // 5. Validate affected equations
      auto affected = getAffectedEquations(boardVec, moveTilesVec);
      for (auto &seq : affected) {
        std::string expr;
        for (auto &t : seq)
          expr += t["value"].asString();

        auto parts = splitEquation(expr);
        if (parts.size() < 1) {
          auto resp = HttpResponse::newHttpJsonResponse(
              PuzzleController::createErrorResponse(
                  "Invalid equation format: " + expr, "VALIDATION_FAILED"));
          resp->setStatusCode(k400BadRequest);
          (*cb)(resp);
          co_return;
        }

        auto val = evaluateExpression(parts[0]);
        for (size_t i = 1; i < parts.size(); ++i) {
          if (evaluateExpression(parts[i]) != val) {
            auto resp = HttpResponse::newHttpJsonResponse(
                PuzzleController::createErrorResponse(
                    "Equation does not hold: " + expr, "VALIDATION_FAILED"));
            resp->setStatusCode(k400BadRequest);
            (*cb)(resp);
            co_return;
          }
        }
      }

      // Move is valid - calculate score
      int score = 0;
      for (const auto &tile : moveTiles) {
        std::string value = tile["value"].asString();
        if (value.length() > 0 && std::isdigit(value[0])) {
          score += std::stoi(value);
        } else {
          score += 1;
        }
      }

      Json::Value respData;
      respData["valid"] = true;
      respData["message"] = "Move is valid!";
      respData["score"] = score;
      respData["streak_updated"] = false;

      auto resp = HttpResponse::newHttpJsonResponse(respData);
      resp->setStatusCode(k200OK);
      (*cb)(resp);

      // Update progress (fire-and-forget after response is sent)
      co_await db->execSqlCoro(
          "INSERT INTO puzzle_progress(user_id, puzzle_id, attempts, "
          "last_attempted_at) "
          "VALUES ($1, $2, 1, NOW()) "
          "ON CONFLICT(user_id, puzzle_id) "
          "DO UPDATE SET attempts = attempts + 1, last_attempted_at = NOW()",
          userId, puzzleId);

    } catch (const DrogonDbException &e) {
      auto resp = HttpResponse::newHttpJsonResponse(
          PuzzleController::createErrorResponse(std::string(e.base().what()),
                                                "SERVER_ERROR"));
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ============================================================================
// POST /puzzle/:puzzleId/submit - Submit puzzle completion
// ============================================================================

void PuzzleController::submitPuzzle(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback, int puzzleId) {
  auto uidAttr = req->attributes()->get<std::string>("uid");
  if (uidAttr.empty()) {
    auto resp =
        HttpResponse::newHttpJsonResponse(PuzzleController::createErrorResponse(
            "Not authenticated", "UNAUTHORIZED"));
    resp->setStatusCode(k401Unauthorized);
    callback(resp);
    return;
  }
  std::string userId = uidAttr;

  Json::Value root;
  Json::CharReaderBuilder builder;
  std::string errs;
  std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  std::string body{req->getBody()};
  if (!reader->parse(body.c_str(), body.c_str() + body.size(), &root, &errs)) {
    auto resp = HttpResponse::newHttpJsonResponse(
        PuzzleController::createErrorResponse("Invalid JSON", "INVALID_DATA"));
    resp->setStatusCode(k400BadRequest);
    callback(resp);
    return;
  }

  int completionTime = root["completion_time"].asInt();
  int finalScore = root["final_score"].asInt();
  int attempts = root["attempts"].asInt();

  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
  std::string completedAt = ss.str();

  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, cb, userId, puzzleId, completionTime, finalScore,
                     attempts, completedAt]() -> drogon::Task<> {
    try {
      co_await db->execSqlCoro(
          "INSERT INTO puzzle_progress(user_id, puzzle_id, solved, attempts, "
          "best_time, best_score, first_completed_at, last_attempted_at) "
          "VALUES ($1, $2, TRUE, $3, $4, $5, NOW(), NOW()) "
          "ON CONFLICT(user_id, puzzle_id) DO UPDATE SET "
          "solved = TRUE, "
          "attempts = COALESCE(puzzle_progress.attempts, 0) + $3, "
          "best_time = LEAST(COALESCE(puzzle_progress.best_time, 999999), $4), "
          "best_score = GREATEST(COALESCE(puzzle_progress.best_score, 0), $5), "
          "last_attempted_at = NOW() "
          "WHERE puzzle_progress.solved = FALSE OR puzzle_progress.best_time > "
          "$4",
          userId, puzzleId, attempts, completionTime, finalScore);

      // Check if this was a daily puzzle
      auto dailyResult = co_await db->execSqlCoro(
          "SELECT is_daily FROM puzzles WHERE puzzle_id = $1", puzzleId);

      if (!dailyResult.empty() && dailyResult[0]["is_daily"].as<bool>()) {
        co_await db->execSqlCoro(
            "INSERT INTO daily_completions (user_id, puzzle_id, "
            "completed_date) "
            "VALUES ($1, $2, CURRENT_DATE) ON CONFLICT DO NOTHING",
            userId, puzzleId);

        co_await db->execSqlCoro(
            "INSERT INTO daily_streak (user_id, current_streak, "
            "longest_streak, last_completed_date) "
            "VALUES ($1, 1, 1, CURRENT_DATE) "
            "ON CONFLICT (user_id) DO UPDATE SET "
            "  current_streak = CASE "
            "    WHEN daily_streak.last_completed_date = CURRENT_DATE - "
            "INTERVAL '1 day' THEN daily_streak.current_streak + 1 "
            "    WHEN daily_streak.last_completed_date = CURRENT_DATE THEN "
            "daily_streak.current_streak "
            "    ELSE 1 END, "
            "  longest_streak = GREATEST(daily_streak.longest_streak, "
            "    CASE WHEN daily_streak.last_completed_date = CURRENT_DATE - "
            "INTERVAL '1 day' THEN "
            "daily_streak.current_streak + 1 ELSE 1 END), "
            "  last_completed_date = CURRENT_DATE "
            "WHERE daily_streak.last_completed_date < CURRENT_DATE",
            userId);
      }

      Json::Value completion;
      completion["puzzle_id"] = puzzleId;
      completion["user_id"] = userId;
      completion["completed_at"] = completedAt;
      completion["completion_time"] = completionTime;
      completion["final_score"] = finalScore;
      completion["attempts"] = attempts;

      Json::Value data;
      data["success"] = true;
      data["message"] = "Puzzle completed successfully";
      data["completion_record"] = completion;

      auto resp = HttpResponse::newHttpJsonResponse(data);
      resp->setStatusCode(k200OK);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      auto resp = HttpResponse::newHttpJsonResponse(
          PuzzleController::createErrorResponse(std::string(e.base().what()),
                                                "SERVER_ERROR"));
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ============================================================================
// GET /puzzles/daily - Get today's daily puzzle
// ============================================================================

void PuzzleController::getDailyPuzzle(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto uidAttr = req->attributes()->get<std::string>("uid");
  if (uidAttr.empty()) {
    auto resp =
        HttpResponse::newHttpJsonResponse(PuzzleController::createErrorResponse(
            "Not authenticated", "UNAUTHORIZED"));
    resp->setStatusCode(k401Unauthorized);
    callback(resp);
    return;
  }
  std::string userId = uidAttr;

  auto now = std::chrono::system_clock::now();
  auto time_t_now = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%d");
  std::string today = ss.str();

  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, cb, userId, today]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT p.puzzle_id, p.difficulty, p.objective, p.board_state, "
          "       p.rack, COALESCE(dc.completed_date IS NOT NULL, FALSE) AS "
          "completed_today "
          "FROM puzzles p "
          "LEFT JOIN daily_completions dc "
          "  ON dc.puzzle_id = p.puzzle_id AND dc.user_id = $1 AND "
          "dc.completed_date = $2 "
          "WHERE p.is_daily = TRUE AND p.daily_date = $2::date "
          "LIMIT 1",
          userId, today);

      if (r.empty()) {
        auto resp = HttpResponse::newHttpJsonResponse(
            PuzzleController::createErrorResponse("No daily puzzle for today",
                                                  "NOT_FOUND"));
        resp->setStatusCode(k404NotFound);
        (*cb)(resp);
        co_return;
      }

      Json::Value boardJson, rackJson;
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

      // Calculate reset time (next UTC midnight)
      auto now = std::chrono::system_clock::now();
      auto time_t_now = std::chrono::system_clock::to_time_t(now);
      std::tm *tm_now = std::gmtime(&time_t_now);
      tm_now->tm_hour = 0;
      tm_now->tm_min = 0;
      tm_now->tm_sec = 0;
      tm_now->tm_mday++;
      std::time_t resetTime = std::mktime(tm_now);

      std::stringstream ss;
      ss << std::put_time(std::gmtime(&resetTime), "%Y-%m-%dT%H:%M:%SZ");
      std::string resetAt = ss.str();

      Json::Value data;
      data["puzzle_id"] = r[0]["puzzle_id"].as<int>();
      data["difficulty"] = r[0]["difficulty"].as<std::string>();
      data["objective"] = r[0]["objective"].as<std::string>();
      data["completed_today"] = r[0]["completed_today"].as<bool>();
      data["reset_at"] = resetAt;
      data["board"] = boardJson;
      data["rack"] = rackJson;

      auto resp = HttpResponse::newHttpJsonResponse(
          PuzzleController::createSuccessResponse(data));
      resp->setStatusCode(k200OK);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      auto resp = HttpResponse::newHttpJsonResponse(
          PuzzleController::createErrorResponse(std::string(e.base().what()),
                                                "SERVER_ERROR"));
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ============================================================================
// GET /puzzles/streak - Get user's daily puzzle streak
// ============================================================================

void PuzzleController::getStreak(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto uidAttr = req->attributes()->get<std::string>("uid");
  if (uidAttr.empty()) {
    auto resp =
        HttpResponse::newHttpJsonResponse(PuzzleController::createErrorResponse(
            "Not authenticated", "UNAUTHORIZED"));
    resp->setStatusCode(k401Unauthorized);
    callback(resp);
    return;
  }
  std::string userId = uidAttr;

  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, cb, userId]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT current_streak, longest_streak, last_completed_date "
          "FROM daily_streak WHERE user_id = $1",
          userId);

      int currentStreak = 0;
      int longestStreak = 0;
      std::string lastCompletedDate = "";

      if (!r.empty()) {
        currentStreak = r[0]["current_streak"].as<int>();
        longestStreak = r[0]["longest_streak"].as<int>();
        if (!r[0]["last_completed_date"].isNull()) {
          lastCompletedDate = r[0]["last_completed_date"].as<std::string>();
        }
      }

      auto result = co_await db->execSqlCoro(
          "SELECT DISTINCT completed_date FROM daily_completions "
          "WHERE user_id = $1 "
          "ORDER BY completed_date DESC LIMIT 30",
          userId);

      Json::Value dailyHistory(Json::arrayValue);
      int totalCompleted = result.size();
      for (const auto &row : result) {
        dailyHistory.append(row["completed_date"].as<std::string>());
      }

      Json::Value data;
      data["user_id"] = userId;
      data["current_streak"] = currentStreak;
      data["longest_streak"] = longestStreak;
      data["last_completed_date"] = lastCompletedDate;
      data["daily_history"] = dailyHistory;
      data["total_daily_completed"] = totalCompleted;

      auto resp = HttpResponse::newHttpJsonResponse(
          PuzzleController::createSuccessResponse(data));
      resp->setStatusCode(k200OK);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      auto resp = HttpResponse::newHttpJsonResponse(
          PuzzleController::createErrorResponse(std::string(e.base().what()),
                                                "SERVER_ERROR"));
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ============================================================================
// GET /user/puzzles/progress - Get user's puzzle completion progress
// ============================================================================

void PuzzleController::getUserProgress(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto uidAttr = req->attributes()->get<std::string>("uid");
  if (uidAttr.empty()) {
    auto resp =
        HttpResponse::newHttpJsonResponse(PuzzleController::createErrorResponse(
            "Not authenticated", "UNAUTHORIZED"));
    resp->setStatusCode(k401Unauthorized);
    callback(resp);
    return;
  }
  std::string userId = uidAttr;

  int limit = 20;
  int offset = 0;

  if (!req->getParameter("limit").empty()) {
    limit = std::stoi(req->getParameter("limit"));
  }
  if (!req->getParameter("offset").empty()) {
    offset = std::stoi(req->getParameter("offset"));
  }

  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, cb, userId, limit, offset]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT puzzle_id, solved, attempts, best_time, best_score, "
          "       first_completed_at, last_attempted_at "
          "FROM puzzle_progress WHERE user_id = $1 "
          "ORDER BY last_attempted_at DESC "
          "LIMIT $2 OFFSET $3",
          userId, limit, offset);

      Json::Value dataArray(Json::arrayValue);
      int totalSolved = 0;
      int totalAttempted = 0;

      for (const auto &row : r) {
        Json::Value item;
        item["puzzle_id"] = row["puzzle_id"].as<int>();
        item["solved"] = row["solved"].as<bool>();
        item["attempts"] = row["attempts"].as<int>();

        if (!row["best_time"].isNull()) {
          item["best_time"] = row["best_time"].as<int>();
        } else {
          item["best_time"] = Json::nullValue;
        }

        if (!row["best_score"].isNull()) {
          item["best_score"] = row["best_score"].as<int>();
        } else {
          item["best_score"] = Json::nullValue;
        }

        if (!row["first_completed_at"].isNull()) {
          item["first_completed"] = row["first_completed_at"].as<std::string>();
        } else {
          item["first_completed"] = Json::nullValue;
        }

        if (!row["last_attempted_at"].isNull()) {
          item["last_attempted"] = row["last_attempted_at"].as<std::string>();
        }

        if (row["solved"].as<bool>())
          totalSolved++;
        totalAttempted++;

        dataArray.append(item);
      }

      Json::Value resp;
      resp["data"] = dataArray;
      resp["total_solved"] = totalSolved;
      resp["total_attempted"] = totalAttempted;

      auto httpResp = HttpResponse::newHttpJsonResponse(resp);
      httpResp->setStatusCode(k200OK);
      (*cb)(httpResp);
    } catch (const DrogonDbException &e) {
      auto resp = HttpResponse::newHttpJsonResponse(
          PuzzleController::createErrorResponse(std::string(e.base().what()),
                                                "SERVER_ERROR"));
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ============================================================================
// GET /user/stats - Get comprehensive user puzzle statistics
// ============================================================================

void PuzzleController::getUserStats(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto uidAttr = req->attributes()->get<std::string>("uid");
  if (uidAttr.empty()) {
    auto resp =
        HttpResponse::newHttpJsonResponse(PuzzleController::createErrorResponse(
            "Not authenticated", "UNAUTHORIZED"));
    resp->setStatusCode(k401Unauthorized);
    callback(resp);
    return;
  }
  std::string userId = uidAttr;

  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, cb, userId]() -> drogon::Task<> {
    try {
      auto userResult = co_await db->execSqlCoro(
          "SELECT display_name FROM users WHERE user_id = $1", userId);

      std::string displayName = "Unknown";
      if (!userResult.empty() && !userResult[0]["display_name"].isNull()) {
        displayName = userResult[0]["display_name"].as<std::string>();
      }

      auto statsResult = co_await db->execSqlCoro(
          "SELECT "
          "  COUNT(CASE WHEN solved = TRUE THEN 1 END) as total_solved, "
          "  COUNT(*) as total_attempted, "
          "  CAST(COUNT(CASE WHEN solved = TRUE THEN 1 END) AS FLOAT) / "
          "    NULLIF(COUNT(*), 0) as solve_rate, "
          "  AVG(best_time) as average_time, "
          "  MIN(best_time) as fastest_time, "
          "  MAX(best_time) as slowest_time "
          "FROM puzzle_progress WHERE user_id = $1",
          userId);

      int totalSolved = 0;
      int totalAttempted = 0;
      double solveRate = 0.0;
      int averageTime = 0;
      int fastestTime = 0;
      int slowestTime = 0;

      if (!statsResult.empty()) {
        auto row = statsResult[0];
        totalSolved = row["total_solved"].as<int>();
        totalAttempted = row["total_attempted"].as<int>();
        solveRate =
            row["solve_rate"].isNull() ? 0.0 : row["solve_rate"].as<double>();
        averageTime =
            row["average_time"].isNull() ? 0 : row["average_time"].as<int>();
        fastestTime =
            row["fastest_time"].isNull() ? 0 : row["fastest_time"].as<int>();
        slowestTime =
            row["slowest_time"].isNull() ? 0 : row["slowest_time"].as<int>();
      }

      auto streakResult =
          co_await db->execSqlCoro("SELECT current_streak, longest_streak FROM "
                                   "daily_streak WHERE user_id = $1",
                                   userId);

      int currentStreak = 0;
      int longestStreak = 0;

      if (!streakResult.empty()) {
        currentStreak = streakResult[0]["current_streak"].as<int>();
        longestStreak = streakResult[0]["longest_streak"].as<int>();
      }

      auto recentResult = co_await db->execSqlCoro(
          "SELECT pp.puzzle_id, pp.best_time, pp.best_score, "
          "pp.first_completed_at "
          "FROM puzzle_progress pp "
          "WHERE pp.user_id = $1 AND pp.solved = TRUE "
          "ORDER BY pp.first_completed_at DESC LIMIT 10",
          userId);

      Json::Value recentArray(Json::arrayValue);
      for (const auto &row : recentResult) {
        Json::Value item;
        item["puzzle_id"] = row["puzzle_id"].as<int>();
        item["solved_at"] = row["first_completed_at"].as<std::string>();
        item["time"] = row["best_time"].as<int>();
        item["score"] = row["best_score"].as<int>();
        recentArray.append(item);
      }

      Json::Value data;
      data["user_id"] = userId;
      data["display_name"] = displayName;
      data["total_solved"] = totalSolved;
      data["total_attempted"] = totalAttempted;
      data["solve_rate"] = solveRate;
      data["average_time"] = averageTime;
      data["fastest_time"] = fastestTime;
      data["slowest_time"] = slowestTime;
      data["longest_streak"] = longestStreak;
      data["current_streak"] = currentStreak;
      data["daily_streak_active"] = currentStreak > 0;
      data["recent_completions"] = recentArray;

      auto resp = HttpResponse::newHttpJsonResponse(
          PuzzleController::createSuccessResponse(data));
      resp->setStatusCode(k200OK);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      auto resp = HttpResponse::newHttpJsonResponse(
          PuzzleController::createErrorResponse(std::string(e.base().what()),
                                                "SERVER_ERROR"));
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}