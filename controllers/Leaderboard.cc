#include "Leaderboard.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/orm/Criteria.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Mapper.h>
#include <drogon/utils/coroutine.h>
#include <memory>
#include <models/Users.h>
#include <trantor/utils/Logger.h>
using namespace drogon::orm;
using namespace drogon_model::neondb;

Leaderboard::Leaderboard() {
  LOG_DEBUG << "Leaderboard controller initialized";
}

void Leaderboard::getLeaderboard(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const {
  auto db = drogon::app().getDbClient();
  auto cb =
      std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(
          std::move(callback));

  drogon::async_run([db, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT uid, name, elo, photo FROM users WHERE elo != 0 ORDER BY elo "
          "DESC LIMIT 100");
      Json::Value root;
      Json::Value jsonPlayers(Json::arrayValue);
      for (const auto &row : r) {
        Json::Value u;
        u["uid"] = row["uid"].as<std::string>();
        u["name"] = row["name"].as<std::string>();
        u["photo"] = row["photo"].as<std::string>();
        u["elo"] = row["elo"].as<int>();
        jsonPlayers.append(u);
      }
      root["players"] = jsonPlayers;
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(root));
    } catch (const DrogonDbException &e) {
      Json::Value error;
      error["error"] = e.base().what();
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(error));
    }
    co_return;
  });
}
