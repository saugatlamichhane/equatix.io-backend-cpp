#pragma once
#include <drogon/HttpController.h>
#include <string>

class GameController : public drogon::HttpController<GameController> {
public:
    METHOD_LIST_BEGIN
    METHOD_ADD(GameController::getReplay, "/{1}/replay", drogon::Get);
    METHOD_LIST_END

    void getReplay(const drogon::HttpRequestPtr &req,
                   std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                   std::string gameId);
};