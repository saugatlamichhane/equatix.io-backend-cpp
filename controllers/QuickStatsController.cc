#include "QuickStatsController.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/orm/Exception.h>
#include <drogon/drogon.h>
#include <json/json.h>

using namespace drogon;

void QuickStatsController::getQuickStats(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const
{
    // Get UID from request attributes (set by FirebaseAuthFilter)
    auto uidAttr = req->attributes()->get<std::string>("uid");
    if (uidAttr.empty())
    {
        Json::Value res;
        res["error"] = "Missing UID";
        callback(HttpResponse::newHttpJsonResponse(res));
        return;
    }
    std::string userId = uidAttr;
    auto client = app().getDbClient();

    // 1. Fetch user's ELO and wins
    client->execSqlAsync(
        "SELECT elo, wins FROM users WHERE uid=$1",
        [client, userId, callback](const drogon::orm::Result &r) {
            Json::Value result;
            if (r.size() == 0)
            {
                result["error"] = "User not found";
                callback(HttpResponse::newHttpJsonResponse(result));
                return;
            }

            result["elo"] = r[0]["elo"].as<int>();
            result["gamesWon"] = r[0]["wins"].as<int>();

            // 2. Count friends
            client->execSqlAsync(
                "SELECT COUNT(*) AS friend_count FROM friends WHERE uid=$1",
                [client, userId, result, callback](const drogon::orm::Result &r2) mutable {
                    result["friends"] = r2[0]["friend_count"].as<int>();

                    // 3. Count active challenges (status='accepted')
                    client->execSqlAsync(
                        "SELECT COUNT(*) AS active_challenges FROM challenges WHERE (challenger_id=$1 OR opponent_id=$1) AND status='accepted'",
                        [result, callback](const drogon::orm::Result &r3) mutable {
                            result["activeChallenges"] = r3[0]["active_challenges"].as<int>();
                            callback(HttpResponse::newHttpJsonResponse(result));
                        },
                        [callback](const drogon::orm::DrogonDbException &e3) {
                            Json::Value res;
                            res["error"] = e3.base().what();
                            callback(HttpResponse::newHttpJsonResponse(res));
                        },
                        userId);
                },
                [callback](const drogon::orm::DrogonDbException &e2) {
                    Json::Value res;
                    res["error"] = e2.base().what();
                    callback(HttpResponse::newHttpJsonResponse(res));
                },
                userId);
        },
        [callback](const drogon::orm::DrogonDbException &e1) {
            Json::Value res;
            res["error"] = e1.base().what();
            callback(HttpResponse::newHttpJsonResponse(res));
        },
        userId);
}
