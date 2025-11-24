#include "LiveStatsController.h"
#include <trantor/utils/Logger.h>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <atomic>

using namespace drogon::orm;

// Initialize static members
std::atomic<size_t> LiveStatsController::activePlayersCount_{0};

void LiveStatsController::incrementActivePlayers() {
    activePlayersCount_++;
}

void LiveStatsController::decrementActivePlayers() {
    if (activePlayersCount_ > 0) {
        activePlayersCount_--;
    }
}

size_t LiveStatsController::getActivePlayersCount() {
    return activePlayersCount_;
}

void LiveStatsController::getLiveStats(const HttpRequestPtr &req,
                                      std::function<void(const HttpResponsePtr &)> &&callback) {
    auto clientPtr = drogon::app().getDbClient();
    auto resp = HttpResponse::newHttpResponse();
    resp->setContentTypeCode(CT_APPLICATION_JSON);

    Json::Value result;

    try {
        // Get uid from request attributes (set by authentication middleware)
        auto uid = req->attributes()->get<std::string>("uid");
        
        if (uid.empty()) {
            resp->setStatusCode(k401Unauthorized);
            result["error"] = "Unauthorized - no user ID found";
            resp->setBody(Json::writeString(Json::StreamWriterBuilder(), result));
            callback(resp);
            return;
        }

        // 1. Get players online count
        size_t playersOnline = getActivePlayersCount();
        result["players_online"] = static_cast<Json::UInt>(playersOnline);

        // 2. Get games today count using PostgreSQL's CURRENT_DATE
        auto gamesTodayResult = clientPtr->execSqlSync(
            "SELECT COUNT(*) as game_count FROM challenges "
            "WHERE status = 'completed' "
            "AND DATE(created_at) = CURRENT_DATE"
        );
        
        int gamesToday = gamesTodayResult[0]["game_count"].as<int>();
        result["games_today"] = gamesToday;

        // 3. Get user's ELO rating
        auto userResult = clientPtr->execSqlSync(
            "SELECT elo FROM users WHERE uid = $1",
            uid
        );
        
        if (userResult.empty()) {
            resp->setStatusCode(k404NotFound);
            result["error"] = "User not found";
            resp->setBody(Json::writeString(Json::StreamWriterBuilder(), result));
            callback(resp);
            return;
        }
        
        double userElo = userResult[0]["elo"].as<double>();
        result["your_rating"] = userElo;

        // 4. Get user's rank (position in leaderboard by ELO)
        auto rankResult = clientPtr->execSqlSync(
            "SELECT COUNT(*) + 1 as rank FROM users u1 "
            "WHERE u1.elo > (SELECT elo FROM users WHERE uid = $1)",
            uid
        );
        
        int userRank = rankResult[0]["rank"].as<int>();
        result["your_rank"] = userRank;


        resp->setStatusCode(k200OK);
        resp->setBody(Json::writeString(Json::StreamWriterBuilder(), result));

    } catch (const DrogonDbException &e) {
        LOG_ERROR << "Database error in getLiveStats: " << e.base().what();
        resp->setStatusCode(k500InternalServerError);
        result["error"] = "Database error";
        resp->setBody(Json::writeString(Json::StreamWriterBuilder(), result));
    } catch (const std::exception &e) {
        LOG_ERROR << "Error in getLiveStats: " << e.what();
        resp->setStatusCode(k500InternalServerError);
        result["error"] = "Internal server error";
        resp->setBody(Json::writeString(Json::StreamWriterBuilder(), result));
    }

    callback(resp);
}