#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/orm/Criteria.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Mapper.h>
#include <models/Users.h>
#include <trantor/utils/Logger.h>
#include "MyProfile.h"

using namespace drogon::orm;
using namespace drogon_model::equatix;

MyProfile::MyProfile() {LOG_DEBUG << "MyProfile controller initialized";}

void MyProfile::getInfo(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback) const {
    auto client = drogon::app().getDbClient();
    auto uid = req->attributes()->get<std::string>("uid");
    LOG_DEBUG << "uid: " << uid;
    client->execSqlAsync("SELECT * FROM users WHERE uid = $1", 
            [callback] (const Result& r) {
                Json::Value u;
                u["uid"] = r[0]["uid"].as<std::string>();
                u["name"] = r[0]["name"].as<std::string>();
                u["elo"] = r[0]["elo"].as<int>();
                u["photo"] = r[0]["photo"].as<std::string>();
                u["gamesplayed"] = r[0]["gamesplayed"].as<int>();
                u["wins"] = r[0]["wins"].as<int>();
                u["losses"] = r[0]["losses"].as<int>();
                u["draws"]= r[0]["draws"].as<int>();
                auto resp = drogon::HttpResponse::newHttpJsonResponse(u);
                callback(resp);
            }, [] (const DrogonDbException& e) {
                LOG_ERROR << e.base().what();
            },
            uid
            );
}
