#include <drogon/HttpAppFramework.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpRequest.h>
#include <drogon/orm/Mapper.h>
#include <drogon/orm/Exception.h>
#include <trantor/utils/Logger.h>
#include <json/json.h>
#include "Clans.h"

using namespace drogon;
using namespace drogon::orm;

Clans::Clans() {
    LOG_INFO << "Clans controller initialized";
}

// ----------------------------------------------------
// CREATE CLAN
// ----------------------------------------------------
void Clans::createClan(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) const {
    auto uid = req->attributes()->get<std::string>("uid");
    auto json = req->getJsonObject();

    // --- Validate input ---
    if (!json || !json->isMember("name")) {
        Json::Value root;
        root["error"] = "Clan name required";
        auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k400BadRequest);
        return callback(resp);
    }

    auto name = (*json)["name"].asString();
    auto client = app().getDbClient();

    // --- Generate a UUID inside SQL and return it ---
    client->execSqlAsync(
        "INSERT INTO clans (id, name, created_by) VALUES (gen_random_uuid(), $1, $2) RETURNING id",
        [callback, uid, client](const Result& r) {
            if (r.size() == 0) {
                Json::Value root;
                root["error"] = "Failed to create clan";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k500InternalServerError);
                return callback(resp);
            }

            // Extract generated clan_id
            std::string clanId = r[0]["id"].as<std::string>();

            // Insert creator as clan leader
            client->execSqlAsync(
                "INSERT INTO clan_members (id, clan_id, user_id, role) "
                "VALUES (gen_random_uuid(), $1, $2, 'leader')",
                [callback, clanId](const Result&) {
                    Json::Value root;
                    root["status"] = "Clan created successfully";
                    root["clan_id"] = clanId;
                    auto resp = HttpResponse::newHttpJsonResponse(root);
                    callback(resp);
                },
                [callback](const DrogonDbException& e) {
                    LOG_ERROR << "Error adding leader to clan_members: " << e.base().what();
                    Json::Value root;
                    root["error"] = e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(root);
                    resp->setStatusCode(k500InternalServerError);
                    callback(resp);
                },
                clanId, uid
            );
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Error creating clan: " << e.base().what();
            Json::Value root;
            root["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        name, uid
    );
}

// ----------------------------------------------------
// GET CLAN INFO
// ----------------------------------------------------
void Clans::getClanInfo(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback,
                        const std::string& id) const {
    auto client = app().getDbClient();

    // --- Fetch clan info first ---
    client->execSqlAsync(
        "SELECT id AS clan_id, name, description, created_by, created_at, "
        "total_points, battles_played, battles_won, battles_lost, battles_tied "
        "FROM clans WHERE id = $1",
        [client, callback, id](const Result& r) {
            Json::Value root;

            if (r.empty()) {
                root["error"] = "Clan not found";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k404NotFound);
                return callback(resp);
            }

            const auto& row = r[0];
            Json::Value clan;
            clan["clan_id"] = row["clan_id"].as<std::string>();
            clan["name"] = row["name"].as<std::string>();
            clan["description"] = row["description"].isNull() ? "" : row["description"].as<std::string>();
            clan["created_by"] = row["created_by"].as<std::string>();
            clan["created_at"] = row["created_at"].as<std::string>();
            clan["total_points"] = row["total_points"].as<int>();
            clan["battles_played"] = row["battles_played"].as<int>();
            clan["battles_won"] = row["battles_won"].as<int>();
            clan["battles_lost"] = row["battles_lost"].as<int>();
            clan["battles_tied"] = row["battles_tied"].as<int>();

            // --- Now load clan members ---
            client->execSqlAsync(
                "SELECT user_id, role, joined_at FROM clan_members WHERE clan_id = $1 ORDER BY joined_at ASC",
                [callback, clan](const Result& members) mutable {
                    Json::Value memberArray(Json::arrayValue);

                    for (const auto& row : members) {
                        Json::Value m;
                        m["user_id"] = row["user_id"].as<std::string>();
                        m["role"] = row["role"].as<std::string>();
                        m["joined_at"] = row["joined_at"].as<std::string>();
                        memberArray.append(m);
                    }

                    Json::Value root;
                    root["clan_info"] = clan;
                    root["members"] = memberArray;

                    auto resp = HttpResponse::newHttpJsonResponse(root);
                    callback(resp);
                },
                [callback](const DrogonDbException& e) {
                    LOG_ERROR << "Error loading clan members: " << e.base().what();
                    Json::Value root;
                    root["error"] = e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(root);
                    resp->setStatusCode(k500InternalServerError);
                    callback(resp);
                },
                id
            );
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Error getting clan info: " << e.base().what();
            Json::Value root;
            root["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        id
    );
}


// ----------------------------------------------------
// GET MY CLAN INFO
void Clans::getMyClanInfo(const HttpRequestPtr& req,
                          std::function<void(const HttpResponsePtr&)>&& callback) const {
    auto uid = req->attributes()->get<std::string>("uid");
    auto client = app().getDbClient();

    // --- Fetch the clan the user belongs to ---
    client->execSqlAsync(
        "SELECT c.id AS clan_id, c.name, c.description, c.created_by, c.created_at, "
        "c.total_points, c.battles_played, c.battles_won, c.battles_lost, c.battles_tied, "
        "m.role, m.joined_at "
        "FROM clans c "
        "JOIN clan_members m ON c.id = m.clan_id "
        "WHERE m.user_id = $1",
        [callback](const Result& r) {
            Json::Value root;

            // --- No clan found ---
            if (r.empty()) {
                root["status"] = "User is not in any clan";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                return callback(resp);
            }

            // --- Build clan info JSON ---
            const auto &row = r[0];
            Json::Value clan;
            clan["clan_id"] = row["clan_id"].as<std::string>();
            clan["name"] = row["name"].as<std::string>();
            clan["description"] = row["description"].isNull() ? "" : row["description"].as<std::string>();
            clan["created_by"] = row["created_by"].as<std::string>();
            clan["created_at"] = row["created_at"].as<std::string>();
            clan["total_points"] = row["total_points"].as<int>();
            clan["battles_played"] = row["battles_played"].as<int>();
            clan["battles_won"] = row["battles_won"].as<int>();
            clan["battles_lost"] = row["battles_lost"].as<int>();
            clan["battles_tied"] = row["battles_tied"].as<int>();

            // --- Add user’s membership info ---
            Json::Value member;
            member["role"] = row["role"].as<std::string>();
            member["joined_at"] = row["joined_at"].as<std::string>();

            // --- Combine ---
            root["clan_info"] = clan;
            root["member_info"] = member;

            auto resp = HttpResponse::newHttpJsonResponse(root);
            callback(resp);
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Error fetching clan info: " << e.base().what();
            Json::Value root;
            root["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        uid
    );
}


// ----------------------------------------------------
// SEND REQUEST
// ----------------------------------------------------
void Clans::sendRequest(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback,
                        const std::string& clanId) const {
    auto uid = req->attributes()->get<std::string>("uid");
    auto client = app().getDbClient();

    client->execSqlAsync(
        "INSERT INTO clan_requests (clan_id, uid) VALUES ($1, $2)",
        [callback](const Result&) {
            Json::Value root;
            root["status"] = "Request sent";
            auto resp = HttpResponse::newHttpJsonResponse(root);
            callback(resp);
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Error sending request: " << e.base().what();
            Json::Value root;
            root["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        clanId, uid
    );
}

// ----------------------------------------------------
// GET REQUESTS (LEADER)
// ----------------------------------------------------
void Clans::getRequests(const HttpRequestPtr& req,
                        std::function<void(const HttpResponsePtr&)>&& callback) const {
    auto uid = req->attributes()->get<std::string>("uid");
    auto client = app().getDbClient();

    client->execSqlAsync(
        "SELECT clan_id FROM clans WHERE leader_uid = $1",
        [client, callback](const Result& r) {
            if (r.empty()) {
                Json::Value root;
                root["error"] = "You are not a clan leader";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k403Forbidden);
                return callback(resp);
            }
            std::string clanId = r[0]["clan_id"].as<std::string>();

            client->execSqlAsync(
                "SELECT * FROM clan_requests WHERE clan_id = $1",
                [callback](const Result& reqs) {
                    Json::Value arr(Json::arrayValue);
                    for (const auto& row : reqs) {
                        Json::Value j;
                        j["uid"] = row["uid"].as<std::string>();
                        arr.append(j);
                    }
                    auto resp = HttpResponse::newHttpJsonResponse(arr);
                    callback(resp);
                },
                [callback](const DrogonDbException& e) {
                    LOG_ERROR << "Error fetching requests: " << e.base().what();
                    Json::Value root;
                    root["error"] = e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(root);
                    resp->setStatusCode(k500InternalServerError);
                    callback(resp);
                },
                clanId
            );
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Error checking leader: " << e.base().what();
            Json::Value root;
            root["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        uid
    );
}

// ----------------------------------------------------
// ACCEPT REQUEST
// ----------------------------------------------------
void Clans::acceptRequest(const HttpRequestPtr& req,
                          std::function<void(const HttpResponsePtr&)>&& callback,
                          const std::string& targetUid) const {
    auto uid = req->attributes()->get<std::string>("uid");
    auto client = app().getDbClient();

    client->execSqlAsync(
        "SELECT clan_id FROM clans WHERE leader_uid = $1",
        [client, callback, targetUid](const Result& r) {
            if (r.empty()) {
                Json::Value root;
                root["error"] = "Not authorized";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k403Forbidden);
                return callback(resp);
            }

            std::string clanId = r[0]["clan_id"].as<std::string>();
            client->execSqlAsync(
                "INSERT INTO clan_members (clan_id, uid, role) VALUES ($1, $2, 'member'); DELETE FROM clan_requests WHERE clan_id=$1 AND uid=$2;",
                [callback](const Result&) {
                    Json::Value root;
                    root["status"] = "Request accepted";
                    auto resp = HttpResponse::newHttpJsonResponse(root);
                    callback(resp);
                },
                [callback](const DrogonDbException& e) {
                    LOG_ERROR << "Error accepting request: " << e.base().what();
                    Json::Value root;
                    root["error"] = e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(root);
                    resp->setStatusCode(k500InternalServerError);
                    callback(resp);
                },
                clanId, targetUid
            );
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Error checking leader for accept: " << e.base().what();
            Json::Value root;
            root["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        uid
    );
}

// ----------------------------------------------------
// REJECT REQUEST
// ----------------------------------------------------
void Clans::rejectRequest(const HttpRequestPtr& req,
                          std::function<void(const HttpResponsePtr&)>&& callback,
                          const std::string& targetUid) const {
    auto uid = req->attributes()->get<std::string>("uid");
    auto client = app().getDbClient();

    client->execSqlAsync(
        "SELECT clan_id FROM clans WHERE leader_uid = $1",
        [client, callback, targetUid](const Result& r) {
            if (r.empty()) {
                Json::Value root;
                root["error"] = "Not authorized";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k403Forbidden);
                return callback(resp);
            }
            std::string clanId = r[0]["clan_id"].as<std::string>();
            client->execSqlAsync(
                "DELETE FROM clan_requests WHERE clan_id=$1 AND uid=$2",
                [callback](const Result&) {
                    Json::Value root;
                    root["status"] = "Request rejected";
                    auto resp = HttpResponse::newHttpJsonResponse(root);
                    callback(resp);
                },
                [callback](const DrogonDbException& e) {
                    LOG_ERROR << "Error rejecting request: " << e.base().what();
                    Json::Value root;
                    root["error"] = e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(root);
                    resp->setStatusCode(k500InternalServerError);
                    callback(resp);
                },
                clanId, targetUid
            );
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Error checking leader for reject: " << e.base().what();
            Json::Value root;
            root["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        uid
    );
}

// ----------------------------------------------------
// INVITES
// ----------------------------------------------------
void Clans::invite(const HttpRequestPtr& req,
                   std::function<void(const HttpResponsePtr&)>&& callback,
                   const std::string& targetUid) const {
    auto uid = req->attributes()->get<std::string>("uid");
    auto client = app().getDbClient();

    client->execSqlAsync(
        "SELECT clan_id FROM clans WHERE leader_uid = $1",
        [client, callback, targetUid](const Result& r) {
            if (r.empty()) {
                Json::Value root;
                root["error"] = "You are not a leader";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k403Forbidden);
                return callback(resp);
            }
            std::string clanId = r[0]["clan_id"].as<std::string>();

            client->execSqlAsync(
                "INSERT INTO clan_invites (clan_id, uid) VALUES ($1, $2)",
                [callback](const Result&) {
                    Json::Value root;
                    root["status"] = "Invite sent";
                    auto resp = HttpResponse::newHttpJsonResponse(root);
                    callback(resp);
                },
                [callback](const DrogonDbException& e) {
                    LOG_ERROR << "Error sending invite: " << e.base().what();
                    Json::Value root;
                    root["error"] = e.base().what();
                    auto resp = HttpResponse::newHttpJsonResponse(root);
                    resp->setStatusCode(k500InternalServerError);
                    callback(resp);
                },
                clanId, targetUid
            );
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Error checking leader for invite: " << e.base().what();
            Json::Value root;
            root["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        uid
    );
}

// ----------------------------------------------------
// GET INVITES
// ----------------------------------------------------
void Clans::getInvites(const HttpRequestPtr& req,
                       std::function<void(const HttpResponsePtr&)>&& callback) const {
    auto uid = req->attributes()->get<std::string>("uid");
    auto client = app().getDbClient();

    client->execSqlAsync(
        "SELECT c.name, c.clan_id FROM clan_invites i JOIN clans c ON i.clan_id = c.clan_id WHERE i.uid = $1",
        [callback](const Result& r) {
            Json::Value arr(Json::arrayValue);
            for (const auto& row : r) {
                Json::Value j;
                j["clan_id"] = row["clan_id"].as<std::string>();
                j["name"] = row["name"].as<std::string>();
                arr.append(j);
            }
            auto resp = HttpResponse::newHttpJsonResponse(arr);
            callback(resp);
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Error fetching invites: " << e.base().what();
            Json::Value root;
            root["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        uid
    );
}

// ----------------------------------------------------
// ACCEPT / REJECT INVITE
// ----------------------------------------------------
void Clans::acceptInvite(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback,
                         const std::string& clanId) const {
    auto uid = req->attributes()->get<std::string>("uid");
    auto client = app().getDbClient();

    client->execSqlAsync(
        "INSERT INTO clan_members (clan_id, uid, role) VALUES ($1, $2, 'member'); DELETE FROM clan_invites WHERE clan_id=$1 AND uid=$2;",
        [callback](const Result&) {
            Json::Value root;
            root["status"] = "Invite accepted";
            auto resp = HttpResponse::newHttpJsonResponse(root);
            callback(resp);
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Error accepting invite: " << e.base().what();
            Json::Value root;
            root["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        clanId, uid
    );
}

void Clans::rejectInvite(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback,
                         const std::string& clanId) const {
    auto uid = req->attributes()->get<std::string>("uid");
    auto client = app().getDbClient();

    client->execSqlAsync(
        "DELETE FROM clan_invites WHERE clan_id=$1 AND uid=$2",
        [callback](const Result&) {
            Json::Value root;
            root["status"] = "Invite rejected";
            auto resp = HttpResponse::newHttpJsonResponse(root);
            callback(resp);
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Error rejecting invite: " << e.base().what();
            Json::Value root;
            root["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(root);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        clanId, uid
    );
}

//

void Clans::kick(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback,
                 const std::string &clanId) const {
    Json::Value root;
    root["status"] = "Kick endpoint not yet implemented";
    auto resp = HttpResponse::newHttpJsonResponse(root);
    callback(resp);
}

void Clans::promote(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback,
                    const std::string &clanId) const {
    Json::Value root;
    root["status"] = "Promote endpoint not yet implemented";
    auto resp = HttpResponse::newHttpJsonResponse(root);
    callback(resp);
}

void Clans::demote(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback,
                   const std::string &clanId) const {
    Json::Value root;
    root["status"] = "Demote endpoint not yet implemented";
    auto resp = HttpResponse::newHttpJsonResponse(root);
    callback(resp);
}

void Clans::search(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback,
                   const std::string &query) const {
    Json::Value root;
    root["status"] = "Search endpoint not yet implemented";
    auto resp = HttpResponse::newHttpJsonResponse(root);
    callback(resp);
}

void Clans::leaveClan(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback) const {
    Json::Value root;
    root["status"] = "Leave clan endpoint not yet implemented";
    auto resp = HttpResponse::newHttpJsonResponse(root);
    callback(resp);
}
