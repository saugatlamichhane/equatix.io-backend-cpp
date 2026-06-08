#include "ChallengeController.h"
#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Result.h>
#include <json/json.h>
#include <optional>
#include <utility>
#include <memory>
#include <drogon/utils/coroutine.h>

using namespace drogon;
using namespace drogon::orm;

namespace {
inline HttpResponsePtr makeJsonResponse(const Json::Value &v) {
  return HttpResponse::newHttpJsonResponse(v);
}

inline HttpResponsePtr makeErrorResponse(const std::string &msg) {
  Json::Value res;
  res["error"] = msg;
  return makeJsonResponse(res);
}

inline std::optional<std::string> getUidAttr(const HttpRequestPtr &req) {
  const auto uid = req->attributes()->get<std::string>("uid");
  if (uid.empty())
    return std::nullopt;
  return uid;
}
} // namespace

// helper types (none required for now)

void ChallengeController::sendChallenge(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &opponentId) {
  auto uidOpt = getUidAttr(req);
  if (!uidOpt) {
    callback(makeErrorResponse("Missing UID"));
    return;
  }
  std::string challengerId = *uidOpt;
  if (challengerId == opponentId) {
    callback(makeErrorResponse("You cannot challenge yourself."));
    return;
  }

  // move callback into coroutine
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(std::move(callback));
  auto db = app().getDbClient();
  std::string opp = opponentId;

  drogon::async_run([db, challengerId = std::move(challengerId), opp = std::move(opp), cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro("SELECT uid FROM users WHERE uid = $1 LIMIT 1", opp);
      if (r.empty()) {
        (*cb)(makeErrorResponse("Opponent not found."));
        co_return;
      }

      auto r2 = co_await db->execSqlCoro(
          "INSERT INTO challenges (challenger_id, opponent_id, status) "
          "SELECT $1::varchar, $2::varchar, 'pending' "
          "WHERE NOT EXISTS (SELECT 1 FROM challenges "
          "WHERE ((challenger_id=$1 AND opponent_id=$2) OR "
          "(challenger_id=$2 AND opponent_id=$1)) "
          "AND status IN ('pending', 'accepted'))",
          challengerId, opp);

      Json::Value res;
      if (r2.affectedRows() == 0) {
        res["error"] = "A challenge already exists between these players.";
      } else {
        res["success"] = true;
        res["message"] = "Challenge sent successfully.";
      }
      (*cb)(makeJsonResponse(res));
    } catch (const DrogonDbException &e) {
      (*cb)(makeErrorResponse(e.base().what()));
    }
    co_return;
  });
}
void ChallengeController::acceptChallenge(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &challengeId) {
  auto uidOpt = getUidAttr(req);
  if (!uidOpt) {
    callback(makeErrorResponse("Missing UID"));
    return;
  }
  std::string receiverId = *uidOpt;
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr & )>>(std::move(callback));

  drogon::async_run([db, challengeId = std::string(challengeId), receiverId = std::move(receiverId), cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro("UPDATE challenges SET status='accepted' WHERE id=$1 AND opponent_id=$2 RETURNING *", challengeId, receiverId);
      Json::Value res;
      if (r.empty())
        res["error"] = "Challenge not found or unauthorized";
      else
        res["message"] = "Challenge accepted";
      (*cb)(makeJsonResponse(res));
    } catch (const DrogonDbException &e) {
      (*cb)(makeErrorResponse(e.base().what()));
    }
    co_return;
  });
}

void ChallengeController::rejectChallenge(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &challengeId) {
  auto uidOpt = getUidAttr(req);
  if (!uidOpt) {
    callback(makeErrorResponse("Missing UID"));
    return;
  }
  std::string receiverId = *uidOpt;
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr & )>>(std::move(callback));

  drogon::async_run([db, challengeId = std::string(challengeId), receiverId = std::move(receiverId), cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro("UPDATE challenges SET status='rejected' WHERE id=$1 AND opponent_id=$2 RETURNING *", challengeId, receiverId);
      Json::Value res;
      if (r.empty())
        res["error"] = "Challenge not found or unauthorized";
      else
        res["message"] = "Challenge rejected";
      (*cb)(makeJsonResponse(res));
    } catch (const DrogonDbException &e) {
      (*cb)(makeErrorResponse(e.base().what()));
    }
    co_return;
  });
}

void ChallengeController::cancelSentChallenge(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &challengeId) {
  auto uidOpt = getUidAttr(req);
  if (!uidOpt) {
    callback(makeErrorResponse("Missing UID"));
    return;
  }
  std::string challengerId = *uidOpt;
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr & )>>(std::move(callback));

  drogon::async_run([db, challengeId = std::string(challengeId), challengerId = std::move(challengerId), cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro("UPDATE challenges SET status='cancelled' WHERE id=$1 AND challenger_id=$2 AND status='pending' RETURNING *", challengeId, challengerId);
      Json::Value res;
      if (r.empty()) {
        res["error"] = "Challenge not found, already started, or unauthorized";
      } else {
        res["message"] = "Challenge cancelled successfully";
      }
      (*cb)(makeJsonResponse(res));
    } catch (const DrogonDbException &e) {
      (*cb)(makeErrorResponse(e.base().what()));
    }
    co_return;
  });
}

void ChallengeController::listSentChallenges(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto uidOpt = getUidAttr(req);
  if (!uidOpt) {
    callback(makeErrorResponse("Missing UID"));
    return;
  }
  std::string userId = *uidOpt;
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr & )>>(std::move(callback));

  drogon::async_run([db, userId = std::move(userId), cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro("SELECT * FROM challenges WHERE challenger_id = $1", userId);
      Json::Value res;
      Json::Value challenges(Json::arrayValue);

      for (const auto &row : r) {
        Json::Value challenge;
        challenge["id"] = row["id"].as<std::string>();
        challenge["opponent_id"] = row["opponent_id"].as<std::string>();
        challenge["challenger_id"] = row["challenger_id"].as<std::string>();
        challenge["status"] = row["status"].as<std::string>();
        challenge["created_at"] = row["created_at"].as<std::string>();
        challenges.append(challenge);
      }

      res["challenges"] = challenges;
      (*cb)(makeJsonResponse(res));
    } catch (const DrogonDbException &e) {
      (*cb)(makeErrorResponse(e.base().what()));
    }
    co_return;
  });
}

void ChallengeController::listReceivedChallenges(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) {
  auto uidOpt = getUidAttr(req);
  if (!uidOpt) {
    callback(makeErrorResponse("Missing UID"));
    return;
  }
  std::string userId = *uidOpt;
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr & )>>(std::move(callback));

  drogon::async_run([db, userId = std::move(userId), cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro("SELECT * FROM challenges WHERE opponent_id = $1", userId);
      Json::Value res;
      Json::Value challenges(Json::arrayValue);

      for (const auto &row : r) {
        Json::Value challenge;
        challenge["id"] = row["id"].as<std::string>();
        challenge["challenger_id"] = row["challenger_id"].as<std::string>();
        challenge["opponent_id"] = row["opponent_id"].as<std::string>();
        challenge["status"] = row["status"].as<std::string>();
        challenge["created_at"] = row["created_at"].as<std::string>();
        challenges.append(challenge);
      }

      res["challenges"] = challenges;
      (*cb)(makeJsonResponse(res));
    } catch (const DrogonDbException &e) {
      (*cb)(makeErrorResponse(e.base().what()));
    }
    co_return;
  });
}
