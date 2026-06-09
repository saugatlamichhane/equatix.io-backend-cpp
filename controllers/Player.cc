#include "Player.h"
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

Player::Player() { LOG_INFO << "Player controller initialized"; }

void Player::getPlayer(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    std::string query) const {
  auto db = drogon::app().getDbClient();
  auto cb =
      std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(
          std::move(callback));
  std::string q = query;
  std::string pattern = "%" + query + "%";

  drogon::async_run([db, q, pattern, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT * FROM users WHERE uid = $1 OR name ILIKE $2", q, pattern);
      Json::Value root;
      Json::Value jsonPlayers(Json::arrayValue);
      for (const auto &row : r) {
        Json::Value jsonPlayer;
        jsonPlayer["name"] = row["name"].as<std::string>();
        jsonPlayer["uid"] = row["uid"].as<std::string>();
        jsonPlayer["photo"] = row["photo"].as<std::string>();
        jsonPlayer["elo"] = row["elo"].as<int>();
        jsonPlayers.append(jsonPlayer);
      }
      root["players"] = jsonPlayers;
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(root));
    } catch (const DrogonDbException &e) {
      LOG_ERROR << "Error executing query: " << e.base().what();
      Json::Value err;
      err["error"] = e.base().what();
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(err));
    }
    co_return;
  });
}
