#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/orm/Criteria.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Mapper.h>
#include <models/Users.h>
#include <trantor/utils/Logger.h>
#include <trantor/utils/Date.h>
#include "MyProfile.h"

using namespace drogon::orm;
using namespace drogon_model::equatix;

MyProfile::MyProfile() {
    LOG_DEBUG << "MyProfile controller initialized";
}

void MyProfile::getInfo(const drogon::HttpRequestPtr &req,
                        std::function<void(const drogon::HttpResponsePtr &)> &&callback) const {
    auto client = drogon::app().getDbClient();
    auto uid = req->attributes()->get<std::string>("uid");
    LOG_DEBUG << "uid: " << uid;

    client->execSqlAsync(
        R"(
            SELECT 
                u.uid, u.name, u.email, u.photo, u.elo,
                u.gamesplayed, u.wins, u.losses, u.draws, u.created_at,
                (SELECT COUNT(*) + 1 FROM users WHERE elo > u.elo) AS rank
            FROM users u
            WHERE u.uid = $1
        )",
        [callback](const Result &r) {
            if (r.empty()) {
                Json::Value error;
                error["error"] = "User not found";
                callback(drogon::HttpResponse::newHttpJsonResponse(error));
                return;
            }

            const auto &row = r[0];
            Json::Value result;
            result["uid"] = row["uid"].as<std::string>();
            result["name"] = row["name"].as<std::string>();
            result["email"] = row["email"].as<std::string>();
            result["photo"] = row["photo"].as<std::string>();
            result["elo"] = row["elo"].as<double>();
            result["gamesplayed"] = row["gamesplayed"].as<int>();
            result["wins"] = row["wins"].as<int>();
            result["losses"] = row["losses"].as<int>();
            result["draws"] = row["draws"].as<int>();
            result["rank"] = row["rank"].as<int>();

            // --- Joined ago calculation ---
            const auto &createdStr = row["created_at"].as<std::string>();
            auto createdDate = trantor::Date::fromDbString(createdStr);
            auto diff = trantor::Date::now().microSecondsSinceEpoch() - createdDate.microSecondsSinceEpoch();
            long long seconds = diff / 1000000;

            std::string joinedAgo;
            if (seconds < 60)
                joinedAgo = std::to_string((int)seconds) + " seconds ago";
            else if (seconds < 3600)
                joinedAgo = std::to_string((int)(seconds / 60)) + " minutes ago";
            else if (seconds < 86400)
                joinedAgo = std::to_string((int)(seconds / 3600)) + " hours ago";
            else if (seconds < 2592000)
                joinedAgo = std::to_string((int)(seconds / 86400)) + " days ago";
            else if (seconds < 31536000)
                joinedAgo = std::to_string((int)(seconds / 2592000)) + " months ago";
            else
                joinedAgo = std::to_string((int)(seconds / 31536000)) + " years ago";

            result["joined_ago"] = joinedAgo;

            callback(drogon::HttpResponse::newHttpJsonResponse(result));
        },
        [callback](const DrogonDbException &e) {
            Json::Value error;
            error["error"] = e.base().what();
            callback(drogon::HttpResponse::newHttpJsonResponse(error));
        },
        uid);
}
