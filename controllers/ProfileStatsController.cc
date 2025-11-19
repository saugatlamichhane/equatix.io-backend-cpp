#include "ProfileStatsController.h"
#include <drogon/drogon.h>

using namespace drogon::orm;
using namespace drogon;

void ProfileStatsController::getStats(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &uid) const
{
    auto db = drogon::app().getDbClient();

    db->execSqlAsync(
        "SELECT users.gamesplayed, stats.current_win_streak, stats.best_elo "
        "FROM users "
        "JOIN stats ON users.uid = stats.uid "
        "WHERE users.uid = $1;",
        [callback](const drogon::orm::Result &r) {
            if (r.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(
                    Json::Value(Json::objectValue));
                resp->setStatusCode(k404NotFound);
                resp->setBody("{\"error\":\"User not found\"}");
                callback(resp);
                return;
            }

            const auto &row = r[0];

            Json::Value json;
            json["gamesplayed"]        = row["gamesplayed"].as<int>();
            json["current_win_streak"] = row["current_win_streak"].as<int>();
            json["best_elo"]           = row["best_elo"].as<int>();

            auto resp = HttpResponse::newHttpJsonResponse(json);
            callback(resp);
        },
        [callback](const DrogonDbException &e) {
            Json::Value err;
            err["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        uid);
}
