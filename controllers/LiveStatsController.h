#pragma once

#include <drogon/HttpController.h>
#include <drogon/orm/DbClient.h>
#include <drogon/drogon.h>
#include <json/json.h>

using namespace drogon;

class LiveStatsController : public HttpController<LiveStatsController> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(LiveStatsController::getLiveStats, "/livestats", Get, "FirebaseAuthFilter");
    METHOD_LIST_END

    void getLiveStats(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);

    // Simple counter methods
    static void incrementActivePlayers();
    static void decrementActivePlayers();
    static size_t getActivePlayersCount();

private:
    static std::atomic<size_t> activePlayersCount_;
};