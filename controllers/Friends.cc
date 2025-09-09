#include "Friends.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/orm/Criteria.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Mapper.h>
#include <models/Users.h>
#include <trantor/utils/Logger.h>

using namespace drogon::orm;
using namespace drogon_model::equatix;

Friends::Friends() { LOG_DEBUG << "Friends controller initialized"; }

void Friends::getFriends(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const {
  auto client = drogon::app().getDbClient();
  auto uid = req->attributes()->get<std::string>("uid");

  client->execSqlAsync(
          "SELECT u.uid, u.name, u.elo FROM friends f join users u on f.friend_uid = u.uid WHERE f.uid = $1",
      [callback, client](const Result &r) {
        Json::Value root;
        Json::Value jsonPlayers(Json::arrayValue);
        for (const auto &row : r) {
        Json::Value u;
        u["uid"] = row["uid"].as<std::string>();
        u["name"] = row["name"].as<std::string>();
        u["elo"]=row["elo"].as<int>();
        jsonPlayers.append(u);

        }
        root["players"] = jsonPlayers;
        auto resp = drogon::HttpResponse::newHttpJsonResponse(root);
        callback(resp);
      },
      [](const DrogonDbException &e) { LOG_ERROR << e.base().what(); }, uid);
}

void Friends::saveFriend(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string friend_uid) {
    auto client = drogon::app().getDbClient();  auto uid = req->attributes()->get<std::string>("uid");
    client->execSqlAsync("INSERT INTO friends(uid, friend_uid) VALUES($1, $2)", [=](const drogon::orm::Result& r) {
                LOG_INFO << "Friend upserted successfully";
            }, [](const drogon::orm::DrogonDbException& e) {
                LOG_DEBUG << "DB Error: " << e.base().what();
            }, uid, friend_uid);

}
