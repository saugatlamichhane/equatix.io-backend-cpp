#include <drogon/HttpAppFramework.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpRequest.h>
#include <drogon/orm/Criteria.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Mapper.h>
#include <models/Users.h>
#include <trantor/utils/Logger.h>
#include "Player.h"

using namespace drogon::orm;
using namespace drogon_model::equatix;

Player::Player() {
    LOG_INFO << "Player controller initialized";
}

void Player::getPlayer(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                       std::string query) const {// Extract player ID from the URL pattern
    auto client = drogon::app().getDbClient();
    client->execSqlAsync(
        "SELECT * FROM users WHERE uid = $1 OR name ILIKE $2",
        [callback](const drogon::orm::Result& r) {
            Json::Value root;
            Json::Value jsonPlayers(Json::arrayValue);
            for(const auto& row : r) {
                Json::Value jsonPlayer;

                jsonPlayer["name"] = row["name"].as<std::string>();
                jsonPlayer["uid"] = row["uid"].as<std::string>();
                jsonPlayer["elo"] = row["elo"].as<int>();
                jsonPlayers.append(jsonPlayer);
            }
            root["players"] = jsonPlayers;
            auto resp = drogon::HttpResponse::newHttpJsonResponse(root);
            callback(resp);
        }, [](const DrogonDbException& e) {
            LOG_ERROR << "Error executing query: " << e.base().what();
        }, query, "%" + query + "%"
    );

}
