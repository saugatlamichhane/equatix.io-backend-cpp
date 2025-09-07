#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/orm/Criteria.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Mapper.h>
#include <models/Users.h>
#include <trantor/utils/Logger.h>
#include "Leaderboard.h"
using namespace drogon::orm;
using namespace drogon_model::equatix;

Leaderboard::Leaderboard() {LOG_DEBUG << "Leaderboard controller initialized";}

void Leaderboard::getLeaderboard(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    auto client = drogon::app().getDbClient();

    client->execSqlAsync(
            "SELECT id, uid, name, elo FROM users WHERE elo != 0 ORDER BY elo DESC LIMIT 100", [callback](const Result& r) {
            Json::Value root;
            Json::Value jsonPlayers(Json::arrayValue);
            for(const auto &row: r) {
                Json::Value u;
                u["id"] = row["id"].as<int>();
                u["uid"] = row["uid"].as<std::string>();
                u["name"] = row["name"].as<std::string>();
                u["elo"] = row["elo"].as<int>();
                jsonPlayers.append(u);
            }
            root["players"] = jsonPlayers;
            auto resp = drogon::HttpResponse::newHttpJsonResponse(root);
            callback(resp);
            }, 
            [callback] (const DrogonDbException& e){
                Json::Value error;
                error["error"] = e.base().what();
                auto resp = drogon::HttpResponse::newHttpJsonResponse(error);
                callback(resp);
            }
            );

}
