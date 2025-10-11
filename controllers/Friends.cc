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
    client->execSqlAsync("INSERT INTO friends(uid, friend_uid) VALUES($1, $2)", [callback](const drogon::orm::Result& r) {
                LOG_INFO << "Friend upserted successfully";
                Json::Value res;
                res["success"] = true;
                res["message"] = "Friend added successfully";
                auto resp = drogon::HttpResponse::newHttpJsonResponse(res);
                callback(resp);
            }, [callback](const drogon::orm::DrogonDbException& e) {
                LOG_DEBUG << "DB Error: " << e.base().what();
                Json::Value err;
                err["success"] = false;
                err["error"] = e.base().what();
                auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
                resp->setStatusCode(drogon::k500InternalServerError);
                callback(resp);
            }, uid, friend_uid);

}

void Friends::deleteFriend(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string friend_uid) {
    auto client = drogon::app().getDbClient();
    auto uid= req->attributes()->get<std::string>("uid");
    client->execSqlAsync("DELETE FROM friends WHERE (uid = $1 AND friend_uid = $2)",
            [callback] (const drogon::orm::Result& r) {
                Json::Value res;
                res["message"] = "Friend deleted successfully";
                auto response = drogon::HttpResponse::newHttpJsonResponse(res);
                callback(response);
            },
            [callback] (const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "DB Error while deleting friend: " << e.base().what();
                Json::Value err;
                err["error"] = "Failed to delete friend";
                auto response = drogon::HttpResponse::newHttpJsonResponse(err);
                callback(response);
            }, uid, friend_uid);
}
