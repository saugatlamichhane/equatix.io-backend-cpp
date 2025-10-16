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

    client->execSqlAsync(
        "SELECT * FROM clans WHERE clan_id = $1",
        [client, callback, id](const Result& r) {
            if (r.empty()) {
                Json::Value root;
                root["error"] = "Clan not found";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                resp->setStatusCode(k404NotFound);
                return callback(resp);
            }

            Json::Value clan;
            clan["clan_id"] = r[0]["clan_id"].as<std::string>();
            clan["name"] = r[0]["name"].as<std::string>();
            clan["leader_uid"] = r[0]["leader_uid"].as<std::string>();
            clan["points"] = r[0]["points"].as<int>();

            client->execSqlAsync(
                "SELECT uid, role FROM clan_members WHERE clan_id = $1",
                [callback, clan](const Result& members) mutable {
                    Json::Value memberArray(Json::arrayValue);
                    for (const auto& row : members) {
                        Json::Value m;
                        m["uid"] = row["uid"].as<std::string>();
                        m["role"] = row["role"].as<std::string>();
                        memberArray.append(m);
                    }
                    clan["members"] = memberArray;
                    auto resp = HttpResponse::newHttpJsonResponse(clan);
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
// ----------------------------------------------------
void Clans::getMyClanInfo(const HttpRequestPtr& req,
                          std::function<void(const HttpResponsePtr&)>&& callback) const {
    auto uid = req->attributes()->get<std::string>("uid");
    auto client = app().getDbClient();

    client->execSqlAsync(
        "SELECT c.* FROM clans c JOIN clan_members m ON c.clan_id = m.clan_id WHERE m.uid = $1",
        [callback](const Result& r) {
            if (r.empty()) {
                Json::Value root;
                root["status"] = "User is not in any clan";
                auto resp = HttpResponse::newHttpJsonResponse(root);
                return callback(resp);
            }

            Json::Value clan;
            clan["clan_id"] = r[0]["clan_id"].as<std::string>();
            clan["name"] = r[0]["name"].as<std::string>();
            clan["points"] = r[0]["points"].as<int>();
            clan["leader_uid"] = r[0]["leader_uid"].as<std::string>();

            auto resp = HttpResponse::newHttpJsonResponse(clan);
            callback(resp);
        },
        [callback](const DrogonDbException& e) {
            LOG_ERROR << "Error fetching my clan: " << e.base().what();
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
