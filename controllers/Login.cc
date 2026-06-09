#include "Login.h"
#include <drogon/utils/coroutine.h>
#include <memory>

Login::Login() {}

void Login::getInfo(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) const {
  auto uid = req->attributes()->get<std::string>("uid");
  auto name = req->attributes()->get<std::string>("name");
  auto email = req->attributes()->get<std::string>("email");
  auto photo = req->attributes()->get<std::string>("photo");

  auto db = drogon::app().getDbClient();
  auto cb =
      std::make_shared<std::function<void(const drogon::HttpResponsePtr &)>>(
          std::move(callback));

  drogon::async_run([db, uid, name, email, photo, cb]() -> drogon::Task<> {
    try {
      auto r = co_await db->execSqlCoro(
          "INSERT INTO users(uid, name, email, photo) VALUES($1, $2, $3, $4) "
          "ON CONFLICT (uid) DO UPDATE SET name = $2, photo = $4",
          uid, name, email, photo);
      Json::Value res;
      res["success"] = true;
      res["uid"] = uid;
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
    } catch (const drogon::orm::DrogonDbException &e) {
      LOG_ERROR << "DB Sync Error: " << e.base().what();
      Json::Value res;
      res["success"] = false;
      res["error"] = "User sync failed";
      (*cb)(drogon::HttpResponse::newHttpJsonResponse(res));
    }
    co_return;
  });
}