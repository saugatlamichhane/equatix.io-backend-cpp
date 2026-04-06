#include "GameController.h"
#include "../models/GameHistory.h"
#include <drogon/orm/Mapper.h>

void GameController::getReplay(const drogon::HttpRequestPtr &req,
                               std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                               int gameId) {
    auto client = drogon::app().getDbClient();
    drogon::orm::Mapper<drogon_model::neondb::GameHistory> mapper(client);

    mapper.findByPrimaryKey(gameId, [callback](drogon_model::neondb::GameHistory g) {
        Json::Value ret = g.toJson();
        // Parse the 'moves' string back into a JSON object for the frontend
        Json::Reader reader;
        Json::Value movesJson;
        if (reader.parse(g.getValueOfMoves(), movesJson)) {
            ret["moves"] = movesJson;
        }
        callback(drogon::HttpResponse::newHttpJsonResponse(ret));
    }, [callback](const drogon::orm::DrogonDbException &e) {
        Json::Value err;
        err["error"] = "Game not found";
        callback(drogon::HttpResponse::newHttpJsonResponse(err));
    });
}