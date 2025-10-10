#include "Profile.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/orm/Exception.h>
#include <drogon/orm/Mapper.h>
#include <models/Users.h>
#include <trantor/utils/Logger.h>

using namespace drogon::orm;
using namespace drogon_model::equatix;

Profile::Profile() { LOG_DEBUG << "Profile controller initialized"; }

void Profile::getInfo(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string& uid) const {
  auto client = drogon::app().getDbClient();
  Mapper<Users> mp(client);

  mp.findByPrimaryKey(
      uid,
      [callback](Users profile) {
        Json::Value result;
                result["name"] = profile.getValueOfName();
                result["uid"] = profile.getValueOfUid();
                result["email"] = profile.getValueOfEmail();
                result["photo"] = profile.getValueOfPhoto();
                result["elo"] = profile.getValueOfElo();
                result["games played"] = profile.getValueOfGamesplayed();
                result["wins"] = profile.getValueOfWins();
                result["losses"] = profile.getValueOfLosses();
                result["draws"] = profile.getValueOfDraws();
                callback(drogon::HttpResponse::newHttpJsonResponse(result));
      },
      [callback](const DrogonDbException &e) {
        Json::Value error;
        error["error"] = e.base().what();
        callback(drogon::HttpResponse::newHttpJsonResponse(error));
      }

  );
}
