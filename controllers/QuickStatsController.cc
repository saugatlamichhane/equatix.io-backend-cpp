#include "QuickStatsController.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/drogon.h>
#include <drogon/orm/Exception.h>
#include <drogon/utils/coroutine.h>
#include <json/json.h>
#include <memory>

using namespace drogon;

void QuickStatsController::getQuickStats(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const {
  auto uidAttr = req->attributes()->get<std::string>("uid");
  if (uidAttr.empty()) {
    Json::Value res;
    res["error"] = "Missing UID";
    callback(HttpResponse::newHttpJsonResponse(res));
    return;
  }
  std::string userId = uidAttr;
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, userId, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT elo, wins FROM users WHERE uid=$1", userId);
      Json::Value result;
      if (r.size() == 0) {
        result["error"] = "User not found";
      } else {
        result["elo"] = r[0]["elo"].as<double>();
        result["gamesWon"] = r[0]["wins"].as<int>();

        auto r2 = co_await db->execSqlCoro(
            "SELECT COUNT(*) AS friend_count FROM friends WHERE uid=$1",
            userId);
        result["friends"] = r2[0]["friend_count"].as<int>();

        auto r3 = co_await db->execSqlCoro(
            "SELECT COUNT(*) AS active_challenges FROM challenges WHERE "
            "(challenger_id=$1 OR opponent_id=$1) AND status='accepted'",
            userId);
        result["activeChallenges"] = r3[0]["active_challenges"].as<int>();
      }
      (*cb)(HttpResponse::newHttpJsonResponse(result));
    } catch (const drogon::orm::DrogonDbException &e) {
      Json::Value err;
      err["error"] = e.base().what();
      (*cb)(HttpResponse::newHttpJsonResponse(err));
    }
    co_return;
  });
}
