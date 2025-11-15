#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/drogon.h>

class QuickStatsController
    : public drogon::HttpController<QuickStatsController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(QuickStatsController::getQuickStats, "/quickstats", drogon::Get,
                "FirebaseAuthFilter");
  METHOD_LIST_END

  void getQuickStats(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback) const;

  QuickStatsController() = default;
};
