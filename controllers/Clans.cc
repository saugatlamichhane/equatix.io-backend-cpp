#include "Clans.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Mapper.h>
#include <drogon/utils/coroutine.h>
#include <json/json.h>
#include <memory>
#include <trantor/utils/Logger.h>

using namespace drogon;
using namespace drogon::orm;

Clans::Clans() { LOG_INFO << "Clans controller initialized"; }

// ----------------------------------------------------
// CREATE CLAN
// ----------------------------------------------------
void Clans::createClan(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const {
  auto uid = req->attributes()->get<std::string>("uid");
  auto json = req->getJsonObject();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));
  auto db = app().getDbClient();

  if (!json || !json->isMember("name")) {
    Json::Value root;
    root["error"] = "Clan name required";
    auto resp = HttpResponse::newHttpJsonResponse(root);
    resp->setStatusCode(k400BadRequest);
    (*cb)(resp);
    return;
  }

  auto name = (*json)["name"].asString();

  drogon::async_run([db, name, uid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "INSERT INTO clans (id, name, created_by) VALUES (gen_random_uuid(), "
          "$1, $2) RETURNING id",
          name, uid);
      if (r.size() == 0) {
        Json::Value root;
        root["error"] = "Failed to create clan";
        auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k500InternalServerError);
        (*cb)(resp);
        co_return;
      }

      std::string clanId = r[0]["id"].as<std::string>();

      co_await db->execSqlCoro(
          "INSERT INTO clan_members (id, clan_id, user_id, role) VALUES "
          "(gen_random_uuid(), $1, $2, 'leader')",
          clanId, uid);

      Json::Value root;
      root["status"] = "Clan created successfully";
      root["clan_id"] = clanId;
      auto resp = HttpResponse::newHttpJsonResponse(root);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      LOG_ERROR << "Error creating clan: " << e.base().what();
      Json::Value root;
      root["error"] = e.base().what();
      auto resp = HttpResponse::newHttpJsonResponse(root);
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ----------------------------------------------------
// GET CLAN INFO
// ----------------------------------------------------
void Clans::getClanInfo(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback,
                        const std::string &id) const {
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, id, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          R"(SELECT id AS clan_id, name, description, created_by, created_at, total_points, battles_played, battles_won, battles_lost, battles_tied FROM clans WHERE id = $1)",
          id);

      if (r.empty()) {
        Json::Value root;
        root["error"] = "Clan not found";
        auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k404NotFound);
        (*cb)(resp);
        co_return;
      }

      const auto &row = r[0];
      Json::Value clan;
      clan["clan_id"] = row["clan_id"].as<std::string>();
      clan["name"] = row["name"].as<std::string>();
      clan["description"] = row["description"].isNull()
                                ? ""
                                : row["description"].as<std::string>();
      clan["created_by"] = row["created_by"].as<std::string>();
      clan["created_at"] = row["created_at"].as<std::string>();
      clan["total_points"] = row["total_points"].as<int>();
      clan["battles_played"] = row["battles_played"].as<int>();
      clan["battles_won"] = row["battles_won"].as<int>();
      clan["battles_lost"] = row["battles_lost"].as<int>();
      clan["battles_tied"] = row["battles_tied"].as<int>();

      auto members = co_await db->execSqlCoro(
          R"(SELECT user_id, role, joined_at FROM clan_members WHERE clan_id = $1 ORDER BY joined_at ASC)",
          id);

      Json::Value memberArray(Json::arrayValue);
      for (const auto &row : members) {
        Json::Value m;
        m["user_id"] = row["user_id"].as<std::string>();
        m["role"] = row["role"].as<std::string>();
        m["joined_at"] = row["joined_at"].as<std::string>();
        memberArray.append(m);
      }

      Json::Value root;
      root["clan_info"] = clan;
      root["members"] = memberArray;
      (*cb)(HttpResponse::newHttpJsonResponse(root));
    } catch (const DrogonDbException &e) {
      LOG_ERROR << "Error getting clan info: " << e.base().what();
      Json::Value root;
      root["error"] = e.base().what();
      auto resp = HttpResponse::newHttpJsonResponse(root);
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ----------------------------------------------------
// GET MY CLAN INFO
// ----------------------------------------------------
void Clans::getMyClanInfo(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const {
  auto uid = req->attributes()->get<std::string>("uid");
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, uid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT c.id AS clan_id, c.name, c.description, c.created_by, "
          "c.created_at, "
          "c.total_points, c.battles_played, c.battles_won, c.battles_lost, "
          "c.battles_tied, "
          "m.role, m.joined_at FROM clans c JOIN clan_members m ON c.id = "
          "m.clan_id "
          "WHERE m.user_id = $1",
          uid);

      Json::Value root;

      if (r.empty()) {
        root["status"] = "User is not in any clan";
        auto resp = HttpResponse::newHttpJsonResponse(root);
        (*cb)(resp);
        co_return;
      }

      const auto &row = r[0];
      Json::Value clan;
      clan["clan_id"] = row["clan_id"].as<std::string>();
      clan["name"] = row["name"].as<std::string>();
      clan["description"] = row["description"].isNull()
                                ? ""
                                : row["description"].as<std::string>();
      clan["created_by"] = row["created_by"].as<std::string>();
      clan["created_at"] = row["created_at"].as<std::string>();
      clan["total_points"] = row["total_points"].as<int>();
      clan["battles_played"] = row["battles_played"].as<int>();
      clan["battles_won"] = row["battles_won"].as<int>();
      clan["battles_lost"] = row["battles_lost"].as<int>();
      clan["battles_tied"] = row["battles_tied"].as<int>();

      Json::Value member;
      member["role"] = row["role"].as<std::string>();
      member["joined_at"] = row["joined_at"].as<std::string>();

      root["clan_info"] = clan;
      root["member_info"] = member;

      auto resp = HttpResponse::newHttpJsonResponse(root);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      LOG_ERROR << "Error fetching clan info: " << e.base().what();
      Json::Value root;
      root["error"] = e.base().what();
      auto resp = HttpResponse::newHttpJsonResponse(root);
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ----------------------------------------------------
// SEND REQUEST
// ----------------------------------------------------
void Clans::sendRequest(const HttpRequestPtr &req,
                        std::function<void(const HttpResponsePtr &)> &&callback,
                        const std::string &clanId) const {
  auto uid = req->attributes()->get<std::string>("uid");
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, uid, clanId, cb]() -> drogon::Task<> {
    try {
      co_await db->execSqlCoro("INSERT INTO clan_join_requests (id, clan_id, "
                               "user_id) VALUES(gen_random_uuid(), $1, $2)",
                               clanId, uid);

      Json::Value root;
      root["status"] = "Request sent";
      auto resp = HttpResponse::newHttpJsonResponse(root);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      LOG_ERROR << "Error sending request: " << e.base().what();
      Json::Value root;
      root["error"] = e.base().what();
      auto resp = HttpResponse::newHttpJsonResponse(root);
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ----------------------------------------------------
// GET REQUESTS (LEADER)
// ----------------------------------------------------
void Clans::getRequests(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const {
  auto uid = req->attributes()->get<std::string>("uid");
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, uid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT id FROM clans WHERE created_by = $1", uid);

      if (r.empty()) {
        Json::Value root;
        root["error"] = "You are not a clan leader";
        auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k403Forbidden);
        (*cb)(resp);
        co_return;
      }

      std::string clanId = r[0]["id"].as<std::string>();
      LOG_DEBUG << "Leader check passed" << " " << clanId;

      auto reqs = co_await db->execSqlCoro(
          "SELECT * FROM clan_join_requests WHERE clan_id = $1", clanId);

      Json::Value arr(Json::arrayValue);
      for (const auto &row : reqs) {
        Json::Value j;
        j["uid"] = row["user_id"].as<std::string>();
        arr.append(j);
      }
      auto resp = HttpResponse::newHttpJsonResponse(arr);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      LOG_ERROR << "Error fetching requests: " << e.base().what();
      Json::Value root;
      root["error"] = e.base().what();
      auto resp = HttpResponse::newHttpJsonResponse(root);
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ----------------------------------------------------
// ACCEPT REQUEST
// ----------------------------------------------------
void Clans::acceptRequest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &targetUid) const {
  auto uid = req->attributes()->get<std::string>("uid");
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, uid, targetUid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT id FROM clans WHERE created_by = $1", uid);

      if (r.empty()) {
        Json::Value root;
        root["error"] = "Not authorized";
        auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k403Forbidden);
        (*cb)(resp);
        co_return;
      }

      std::string clanId = r[0]["id"].as<std::string>();

      co_await db->execSqlCoro(
          "INSERT INTO clan_members (id, clan_id, user_id, role) VALUES "
          "(gen_random_uuid(), $1, $2, 'member')",
          clanId, targetUid);

      co_await db->execSqlCoro(
          "DELETE FROM clan_join_requests WHERE clan_id=$1 AND user_id=$2",
          clanId, targetUid);

      Json::Value root;
      root["status"] = "Request accepted";
      auto resp = HttpResponse::newHttpJsonResponse(root);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      LOG_ERROR << "Error accepting request: " << e.base().what();
      Json::Value root;
      root["error"] = e.base().what();
      auto resp = HttpResponse::newHttpJsonResponse(root);
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ----------------------------------------------------
// REJECT REQUEST
// ----------------------------------------------------
void Clans::rejectRequest(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &targetUid) const {
  auto uid = req->attributes()->get<std::string>("uid");
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, uid, targetUid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT id FROM clans WHERE created_by = $1", uid);

      if (r.empty()) {
        Json::Value root;
        root["error"] = "Not authorized";
        auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k403Forbidden);
        (*cb)(resp);
        co_return;
      }

      std::string clanId = r[0]["id"].as<std::string>();

      co_await db->execSqlCoro(
          "DELETE FROM clan_join_requests WHERE clan_id=$1 AND user_id=$2",
          clanId, targetUid);

      Json::Value root;
      root["status"] = "Request rejected";
      auto resp = HttpResponse::newHttpJsonResponse(root);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      LOG_ERROR << "Error checking leader for reject: " << e.base().what();
      Json::Value root;
      root["error"] = e.base().what();
      auto resp = HttpResponse::newHttpJsonResponse(root);
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ----------------------------------------------------
// INVITES
// ----------------------------------------------------
void Clans::invite(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback,
                   const std::string &targetUid) const {
  auto uid = req->attributes()->get<std::string>("uid");
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));
  LOG_DEBUG << "Invite called by " << uid << " to " << targetUid;

  drogon::async_run([db, uid, targetUid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT id FROM clans WHERE created_by = $1", uid);

      if (r.empty()) {
        Json::Value root;
        root["error"] = "You are not a leader";
        auto resp = HttpResponse::newHttpJsonResponse(root);
        resp->setStatusCode(k403Forbidden);
        (*cb)(resp);
        co_return;
      }

      std::string clanId = r[0]["id"].as<std::string>();

      co_await db->execSqlCoro(
          "INSERT INTO clan_invites (id, clan_id, invited_user_id, invited_by) "
          "VALUES (gen_random_uuid(), $1, $2, $3)",
          clanId, targetUid, uid);

      Json::Value root;
      root["status"] = "Invite sent";
      auto resp = HttpResponse::newHttpJsonResponse(root);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      LOG_ERROR << "Error checking leader for invite: " << e.base().what();
      Json::Value root;
      root["error"] = e.base().what();
      auto resp = HttpResponse::newHttpJsonResponse(root);
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ----------------------------------------------------
// GET INVITES
// ----------------------------------------------------
void Clans::getInvites(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const {
  auto uid = req->attributes()->get<std::string>("uid");
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, uid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT c.name, c.id FROM clan_invites i JOIN clans c ON i.clan_id = "
          "c.id WHERE i.invited_user_id = $1",
          uid);

      Json::Value arr(Json::arrayValue);
      for (const auto &row : r) {
        Json::Value j;
        j["clan_id"] = row["id"].as<std::string>();
        j["name"] = row["name"].as<std::string>();
        arr.append(j);
      }
      auto resp = HttpResponse::newHttpJsonResponse(arr);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      LOG_ERROR << "Error fetching invites: " << e.base().what();
      Json::Value root;
      root["error"] = e.base().what();
      auto resp = HttpResponse::newHttpJsonResponse(root);
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ----------------------------------------------------
// ACCEPT / REJECT INVITE
// ----------------------------------------------------
void Clans::acceptInvite(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &clanId) const {
  auto uid = req->attributes()->get<std::string>("uid");
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, uid, clanId, cb]() -> drogon::Task<> {
    try {
      co_await db->execSqlCoro(
          "INSERT INTO clan_members (id, clan_id, user_id, role) VALUES "
          "(gen_random_uuid(), $1, $2, 'member')",
          clanId, uid);

      co_await db->execSqlCoro(
          "DELETE FROM clan_invites WHERE clan_id=$1 AND invited_user_id=$2",
          clanId, uid);

      Json::Value root;
      root["status"] = "Invite accepted";
      auto resp = HttpResponse::newHttpJsonResponse(root);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      LOG_ERROR << "Error accepting invite: " << e.base().what();
      Json::Value root;
      root["error"] = e.base().what();
      auto resp = HttpResponse::newHttpJsonResponse(root);
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

void Clans::rejectInvite(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    const std::string &clanId) const {
  auto uid = req->attributes()->get<std::string>("uid");
  auto db = app().getDbClient();
  auto cb = std::make_shared<std::function<void(const HttpResponsePtr &)>>(
      std::move(callback));

  drogon::async_run([db, uid, clanId, cb]() -> drogon::Task<> {
    try {
      co_await db->execSqlCoro(
          "DELETE FROM clan_invites WHERE clan_id=$1 AND invited_user_id=$2",
          clanId, uid);

      Json::Value root;
      root["status"] = "Invite rejected";
      auto resp = HttpResponse::newHttpJsonResponse(root);
      (*cb)(resp);
    } catch (const DrogonDbException &e) {
      LOG_ERROR << "Error rejecting invite: " << e.base().what();
      Json::Value root;
      root["error"] = e.base().what();
      auto resp = HttpResponse::newHttpJsonResponse(root);
      resp->setStatusCode(k500InternalServerError);
      (*cb)(resp);
    }
    co_return;
  });
}

// ----------------------------------------------------
// STUBS
// ----------------------------------------------------
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

void Clans::leaveClan(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback) const {
  Json::Value root;
  root["status"] = "Leave clan endpoint not yet implemented";
  auto resp = HttpResponse::newHttpJsonResponse(root);
  callback(resp);
}