#include "Friends.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/orm/Exception.h>
#include <drogon/utils/coroutine.h>
#include <json/json.h>
#include <memory>
#include <models/Users.h>
#include <trantor/utils/Logger.h>

using namespace drogon::orm;
using namespace drogon_model::neondb;

Friends::Friends() { LOG_DEBUG << "Friends controller initialized"; }

void Friends::getFriends(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto db = drogon::app().getDbClient();
  auto uid = req->attributes()->get<std::string>("uid");
  auto cb =
      std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(
          std::move(callback));

  drogon::async_run([db, uid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT u.uid, u.name, u.elo, u.photo FROM friends f JOIN users u ON "
          "f.friend_uid = u.uid WHERE f.uid = $1 AND f.status='accepted'",
          uid);
      Json::Value root;
      Json::Value arr(Json::arrayValue);
      for (const auto &row : r) {
        Json::Value u;
        u["uid"] = row["uid"].as<std::string>();
        u["name"] = row["name"].as<std::string>();
        u["elo"] = row["elo"].as<int>();
        u["photo"] = row["photo"].as<std::string>();
        arr.append(u);
      }
      root["friends"] = arr;
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(root));
    } catch (const DrogonDbException &e) {
      Json::Value err;
      err["error"] = e.base().what();
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(err));
    }
    co_return;
  });
}

void Friends::getFriendRequests(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto db = drogon::app().getDbClient();
  auto uid = req->attributes()->get<std::string>("uid");
  auto cb =
      std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(
          std::move(callback));

  drogon::async_run([db, uid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT u.uid, u.name, u.elo, u.photo FROM friends f JOIN users u ON "
          "f.requested_by = u.uid WHERE f.friend_uid=$1 AND f.status='pending'",
          uid);
      Json::Value root;
      Json::Value arr(Json::arrayValue);
      for (const auto &row : r) {
        Json::Value u;
        u["uid"] = row["uid"].as<std::string>();
        u["name"] = row["name"].as<std::string>();
        u["elo"] = row["elo"].as<int>();
        u["photo"] = row["photo"].as<std::string>();
        arr.append(u);
      }
      root["requests"] = arr;
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(root));
    } catch (const DrogonDbException &e) {
      Json::Value err;
      err["error"] = e.base().what();
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(err));
    }
    co_return;
  });
}

void Friends::sendFriendRequest(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    std::string friend_uid) {
  auto db = drogon::app().getDbClient();
  auto uid = req->attributes()->get<std::string>("uid");
  auto cb =
      std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(
          std::move(callback));

  if (uid == friend_uid) {
    Json::Value res;
    res["success"] = false;
    res["error"] = "Cannot add yourself";
    (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
    return;
  }

  drogon::async_run([db, uid, friend_uid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT uid FROM users WHERE uid=$1 LIMIT 1", friend_uid);
      if (r.empty()) {
        Json::Value res;
        res["success"] = false;
        res["error"] = "User not found";
        (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
        co_return;
      }

      auto r2 = co_await db->execSqlCoro(
          "SELECT 1 FROM friends WHERE (uid=$1 AND friend_uid=$2) OR (uid=$2 "
          "AND friend_uid=$1) LIMIT 1",
          uid, friend_uid);
      if (!r2.empty()) {
        Json::Value res;
        res["success"] = false;
        res["error"] = "Friendship or request already exists";
        (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
        co_return;
      }

      auto r3 = co_await db->execSqlCoro(
          "INSERT INTO friends(uid, friend_uid, status, requested_by) "
          "VALUES($1,$2,'pending',$3)",
          uid, friend_uid, uid);
      Json::Value res;
      res["success"] = true;
      res["message"] = "Friend request sent";
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
    } catch (const DrogonDbException &e) {
      Json::Value res;
      res["success"] = false;
      res["error"] = e.base().what();
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
    }
    co_return;
  });
}

void Friends::getSentFriendRequests(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto db = drogon::app().getDbClient();
  auto uid = req->attributes()->get<std::string>("uid");
  auto cb =
      std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(
          std::move(callback));

  drogon::async_run([db, uid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "SELECT u.uid, u.name, u.elo, u.photo FROM friends f JOIN users u ON "
          "f.friend_uid = u.uid WHERE f.uid=$1 AND f.status='pending'",
          uid);
      Json::Value root;
      Json::Value arr(Json::arrayValue);
      for (const auto &row : r) {
        Json::Value u;
        u["uid"] = row["uid"].as<std::string>();
        u["name"] = row["name"].as<std::string>();
        u["elo"] = row["elo"].as<int>();
        u["photo"] = row["photo"].as<std::string>();
        arr.append(u);
      }
      root["sentRequests"] = arr;
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(root));
    } catch (const DrogonDbException &e) {
      Json::Value err;
      err["error"] = e.base().what();
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(err));
    }
    co_return;
  });
}

void Friends::cancelSentFriendRequest(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    std::string friend_uid) {
  auto db = drogon::app().getDbClient();
  auto uid = req->attributes()->get<std::string>("uid");
  auto cb =
      std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(
          std::move(callback));

  drogon::async_run([db, uid, friend_uid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro("DELETE FROM friends WHERE uid=$1 AND "
                                        "friend_uid=$2 AND status='pending'",
                                        uid, friend_uid);
      Json::Value res;
      res["success"] = true;
      res["message"] = "Friend request canceled";
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
    } catch (const DrogonDbException &e) {
      Json::Value res;
      res["success"] = false;
      res["error"] = e.base().what();
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
    }
    co_return;
  });
}

void Friends::acceptFriendRequest(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    std::string friend_uid) {
  auto db = drogon::app().getDbClient();
  auto uid = req->attributes()->get<std::string>("uid");
  auto cb =
      std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(
          std::move(callback));

  drogon::async_run([db, uid, friend_uid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "UPDATE friends SET status='accepted' WHERE uid=$1 AND friend_uid=$2 "
          "AND status='pending'",
          uid, friend_uid);
      if (r.affectedRows() == 0) {
        Json::Value res;
        res["success"] = false;
        res["error"] = "No pending request found";
        (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
        co_return;
      }
      auto r2 = co_await db->execSqlCoro(
          "INSERT INTO friends(uid, friend_uid, status, requested_by) "
          "VALUES($1,$2,'accepted',$3)",
          uid, friend_uid, friend_uid);
      Json::Value res;
      res["success"] = true;
      res["message"] = "Friend request accepted";
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
    } catch (const DrogonDbException &e) {
      Json::Value res;
      res["success"] = false;
      res["error"] = e.base().what();
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
    }
    co_return;
  });
}

void Friends::rejectFriendRequest(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    std::string friend_uid) {
  auto db = drogon::app().getDbClient();
  auto uid = req->attributes()->get<std::string>("uid");
  auto cb =
      std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(
          std::move(callback));

  drogon::async_run([db, uid, friend_uid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro("DELETE FROM friends WHERE uid=$1 AND "
                                        "friend_uid=$2 AND status='pending'",
                                        uid, friend_uid);
      Json::Value res;
      res["success"] = true;
      res["message"] = "Friend request rejected";
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
    } catch (const DrogonDbException &e) {
      Json::Value res;
      res["success"] = false;
      res["error"] = e.base().what();
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
    }
    co_return;
  });
}

void Friends::deleteFriend(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    std::string friend_uid) {
  auto db = drogon::app().getDbClient();
  auto uid = req->attributes()->get<std::string>("uid");
  auto cb =
      std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(
          std::move(callback));

  drogon::async_run([db, uid, friend_uid, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "DELETE FROM friends WHERE (uid=$1 AND friend_uid=$2) OR (uid=$2 AND "
          "friend_uid=$1)",
          uid, friend_uid);
      Json::Value res;
      res["success"] = true;
      res["message"] = "Friend deleted";
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
    } catch (const DrogonDbException &e) {
      Json::Value res;
      res["success"] = false;
      res["error"] = e.base().what();
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
    }
    co_return;
  });
}
