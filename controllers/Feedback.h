#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/drogon.h>

class Feedback : public drogon::HttpController<Feedback> {
public:
  METHOD_LIST_BEGIN
  METHOD_ADD(Feedback::sendFeedback, "", drogon::Post);
  METHOD_LIST_END

  void
  sendFeedback(const drogon::HttpRequestPtr &req,
               std::function<void(const drogon::HttpResponsePtr &)> &&callback);

  Feedback();
};
