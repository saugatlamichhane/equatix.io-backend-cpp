#include "Profile.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Mapper.h>
#include <models/Users.h>
#include <trantor/utils/Date.h>
#include <trantor/utils/Logger.h>

using namespace drogon::orm;
using namespace drogon_model::equatix;

Profile::Profile() { LOG_DEBUG << "Profile controller initialized"; }

void Profile::getInfo(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &uid) const
{
    auto client = drogon::app().getDbClient();

    client->execSqlAsync(
R"(
        SELECT 
            u.uid, u.name, u.email, u.photo, u.elo,
            u.gamesplayed, u.wins, u.losses, u.draws, u.created_at,
            s.best_elo, s.best_win_streak, s.current_win_streak,
            (SELECT COUNT(*) + 1 FROM users WHERE elo > u.elo) AS rank,
            COALESCE(SUM(EXTRACT(EPOCH FROM c.completed_at - c.created_at)), 0) AS total_game_seconds,
            COALESCE(AVG(EXTRACT(EPOCH FROM c.completed_at - c.created_at)), 0) AS avg_game_seconds
        FROM users u
        LEFT JOIN stats s ON u.uid = s.uid
        -- Join challenges where the user was a participant and the game is completed
        LEFT JOIN challenges c ON 
            (u.uid = c.challenger_id OR u.uid = c.opponent_id) AND c.status = 'completed' AND c.completed_at IS NOT NULL
        WHERE u.uid = $1
        GROUP BY u.uid, u.name, u.email, u.photo, u.elo, u.gamesplayed, u.wins, u.losses, u.draws, u.created_at, s.best_elo, s.best_win_streak, s.current_win_streak
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
            result["games played"] = row["gamesplayed"].as<int>();
            result["wins"] = row["wins"].as<int>();
            result["losses"] = row["losses"].as<int>();
            result["draws"] = row["draws"].as<int>();
            result["rank"] = row["rank"].as<int>();

            // Stats from stats table
            result["bestElo"] = row["best_elo"].isNull() ? row["elo"].as<double>() : row["best_elo"].as<int>();
            result["bestWinStreak"] = row["best_win_streak"].isNull() ? 0 : row["best_win_streak"].as<int>();
            result["currentStreak"] = row["current_win_streak"].isNull() ? 0 : row["current_win_streak"].as<int>();

            // Calculate "joined ago"
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

            long long totalGameSeconds = row["total_game_seconds"].as<long long>();
            double avgGameSeconds = row["avg_game_seconds"].as<double>();
            result["avgGameTimeMinutes"] = round((avgGameSeconds / 60.0) * 10) / 10.0; // rounded to 2 decimal places
            result["totalPlayTimeHours"] = (int) round((totalGameSeconds / 3600.0) * 10) / 10.0; // rounded to 2 decimal places
            callback(drogon::HttpResponse::newHttpJsonResponse(result));
        },
        [callback](const DrogonDbException &e) {
            Json::Value error;
            error["error"] = e.base().what();
            callback(drogon::HttpResponse::newHttpJsonResponse(error));
        },
        uid);
}
