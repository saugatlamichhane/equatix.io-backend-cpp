#pragma once

#include <drogon/HttpController.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/drogon.h>

class Player : public drogon::HttpController<Player> {
public:
  METHOD_LIST_BEGIN

  METHOD_ADD(Player::getPlayer, "/{1}",
             drogon::Get); // path is /Player/{arg1}/{arg2}
  METHOD_LIST_END

  void
  getPlayer(const drogon::HttpRequestPtr &req,
            std::function<void(const drogon::HttpResponsePtr &)> &&callback,
            std::string query) const;
  Player();
};