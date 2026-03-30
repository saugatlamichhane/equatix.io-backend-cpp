#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/drogon.h>

class Leaderboard : public drogon::HttpController<Leaderboard> {
public:
  METHOD_LIST_BEGIN
  METHOD_ADD(Leaderboard::getLeaderboard, "", drogon::Get);
  METHOD_LIST_END

  void getLeaderboard(
      const drogon::HttpRequestPtr &req,
      std::function<void(const drogon::HttpResponsePtr &)> &&callback) const;
  Leaderboard();
};
