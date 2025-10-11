#include "ChallengeController.h"
#include <drogon/drogon.h>
#include <json/json.h>
#include <drogon/orm/Result.h>
#include <drogon/orm/Exception.h>

using namespace drogon;
using namespace drogon::orm;

void ChallengeController::sendChallenge(const HttpRequestPtr &req,
                                        std::function<void(const HttpResponsePtr &)> &&callback,
                                        const std::string &opponentId) {
    auto uidAttr = req->attributes()->get<std::string>("uid");
    if (uidAttr.empty()) {
        Json::Value res;
        res["error"] = "Missing UID";
        auto resp = HttpResponse::newHttpJsonResponse(res);
        callback(resp);
        return;
    }

    auto client = app().getDbClient();
    std::string challengerId = uidAttr;

    client->execSqlAsync(
        "INSERT INTO challenges (challenger_id, opponent_id, status) SELECT $1, $2, 'pending' WHERE NOT EXISTS (SELECT 1 FROM challenges WHERE ((challenger_id=$1 AND opponent_id=$2) OR (challenger_id=$2 AND opponent_id=$1)) AND status IN ('pending', 'accepted'))",
        [callback](const Result &r) {
            Json::Value res;
            res["message"] = "Challenge sent successfully";
            auto resp = HttpResponse::newHttpJsonResponse(res);
            callback(resp);
        },
        [callback](const DrogonDbException &e) {
            Json::Value res;
            res["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(res);
            callback(resp);
        },
        challengerId, opponentId);
}

void ChallengeController::acceptChallenge(const HttpRequestPtr &req,
                                          std::function<void(const HttpResponsePtr &)> &&callback,
                                          const std::string &challengeId) {
    auto uidAttr = req->attributes()->get<std::string>("uid");
    if (uidAttr.empty()) {
        Json::Value res;
        res["error"] = "Missing UID";
        auto resp = HttpResponse::newHttpJsonResponse(res);
        callback(resp);
        return;
    }

    std::string receiverId = uidAttr;
    auto client = app().getDbClient();

    client->execSqlAsync(
        "UPDATE challenges SET status='accepted' WHERE id=$1 AND opponent_id=$2 RETURNING *",
        [callback](const Result &r) {
            Json::Value res;
            if (r.empty())
                res["error"] = "Challenge not found or unauthorized";
            else
                res["message"] = "Challenge accepted";
            auto resp = HttpResponse::newHttpJsonResponse(res);
            callback(resp);
        },
        [callback](const DrogonDbException &e) {
            Json::Value res;
            res["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(res);
            callback(resp);
        },
        challengeId, receiverId);
}

void ChallengeController::rejectChallenge(const HttpRequestPtr &req,
                                          std::function<void(const HttpResponsePtr &)> &&callback,
                                          const std::string &challengeId) {
    auto uidAttr = req->attributes()->get<std::string>("uid");
    if (uidAttr.empty()) {
        Json::Value res;
        res["error"] = "Missing UID";
        auto resp = HttpResponse::newHttpJsonResponse(res);
        callback(resp);
        return;
    }

    std::string receiverId = uidAttr;
    auto client = app().getDbClient();

    client->execSqlAsync(
        "UPDATE challenges SET status='rejected' WHERE id=$1 AND opponent_id=$2 RETURNING *",
        [callback](const Result &r) {
            Json::Value res;
            if (r.empty())
                res["error"] = "Challenge not found or unauthorized";
            else
                res["message"] = "Challenge rejected";
            auto resp = HttpResponse::newHttpJsonResponse(res);
            callback(resp);
        },
        [callback](const DrogonDbException &e) {
            Json::Value res;
            res["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(res);
            callback(resp);
        },
        challengeId, receiverId);
}

void ChallengeController::listSentChallenges(const HttpRequestPtr &req,
                                             std::function<void(const HttpResponsePtr &)> &&callback) {
    auto uidAttr = req->attributes()->get<std::string>("uid");
    if (uidAttr.empty()) {
        Json::Value res;
        res["error"] = "Missing UID";
        auto resp = HttpResponse::newHttpJsonResponse(res);
        callback(resp);
        return;
    }

    std::string userId = uidAttr;
    auto client = app().getDbClient();

    client->execSqlAsync(
        "SELECT * FROM challenges WHERE challenger_id = $1",
        [callback](const Result &r) {
            Json::Value res;
            Json::Value challenges(Json::arrayValue);
            
            for (const auto &row : r) {
                Json::Value challenge;
                challenge["id"] = row["id"].as<std::string>();
                challenge["opponent_id"] = row["opponent_id"].as<std::string>();
                challenge["status"] = row["status"].as<std::string>();
                challenge["created_at"] = row["created_at"].as<std::string>();
                challenges.append(challenge);
            }
            
            res["challenges"] = challenges;
            auto resp = HttpResponse::newHttpJsonResponse(res);
            callback(resp);
        },
        [callback](const DrogonDbException &e) {
            Json::Value res;
            res["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(res);
            callback(resp);
        },
        userId);
}

void ChallengeController::listReceivedChallenges(const HttpRequestPtr &req,
                                                std::function<void(const HttpResponsePtr &)> &&callback) {
    auto uidAttr = req->attributes()->get<std::string>("uid");
    if (uidAttr.empty()) {
        Json::Value res;
        res["error"] = "Missing UID";
        auto resp = HttpResponse::newHttpJsonResponse(res);
        callback(resp);
        return;
    }

    std::string userId = uidAttr;
    auto client = app().getDbClient();

    client->execSqlAsync(
        "SELECT * FROM challenges WHERE opponent_id = $1",
        [callback](const Result &r) {
            Json::Value res;
            Json::Value challenges(Json::arrayValue);
            
            for (const auto &row : r) {
                Json::Value challenge;
                challenge["id"] = row["id"].as<std::string>();
                challenge["challenger_id"] = row["challenger_id"].as<std::string>();
                challenge["status"] = row["status"].as<std::string>();
                challenge["created_at"] = row["created_at"].as<std::string>();
                challenges.append(challenge);
            }
            
            res["challenges"] = challenges;
            auto resp = HttpResponse::newHttpJsonResponse(res);
            callback(resp);
        },
        [callback](const DrogonDbException &e) {
            Json::Value res;
            res["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(res);
            callback(resp);
        },
        userId);
}
