#include "Feedback.h"
#include <drogon/HttpAppFramework.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>
#include <drogon/orm/Mapper.h>
#include <models/Feedbacks.h>
#include <trantor/utils/Logger.h>
using namespace drogon::orm;
using namespace drogon_model::equatix;

Feedback::Feedback() { LOG_DEBUG << "Feedback controller initialized!"; }

void Feedback::sendFeedback(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
  auto client = drogon::app().getDbClient();
  auto mp = drogon::orm::Mapper<Feedbacks>(client);

  Feedbacks newFeedback;
  auto json = req->getJsonObject();
  if (!json || !(*json).isMember("message")) {
    LOG_DEBUG << "Missing 'message' field.";
    return;
  }

  std::string feedback = (*json)["message"].asString();
  int rating = (*json)["rating"].asInt();
  newFeedback.setMessage(feedback);
  newFeedback.setRating(rating);

  mp.insert(
      newFeedback,
      [callback](Feedbacks feedback) {
        Json::Value result;
        result["message"] = feedback.getValueOfMessage();
        result["rating"] = feedback.getValueOfRating();
        auto resp = drogon::HttpResponse::newHttpJsonResponse(result);
        resp->setStatusCode(drogon::k201Created);
        callback(resp);
      },
      [callback](const DrogonDbException &e) {
        LOG_DEBUG << "Error Inserting Feedback: " << e.base().what();
      });
}
