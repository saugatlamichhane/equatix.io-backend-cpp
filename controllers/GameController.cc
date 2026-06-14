#include "GameController.h"
#include "../models/GameHistory.h"
#include <drogon/orm/Mapper.h>
#include <json/json.h>
#include <memory>

void GameController::getReplay(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    std::string gameId) {
  auto client = drogon::app().getDbClient();
  drogon::orm::Mapper<drogon_model::neondb::GameHistory> mapper(client);

  mapper.findByPrimaryKey(
      gameId,
      [callback](drogon_model::neondb::GameHistory g) {
        Json::Value ret = g.toJson();
        // Parse the 'moves' string back into a JSON object for the frontend
        Json::CharReaderBuilder builder;
        std::string errs;
        std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
        Json::Value movesJson;
        const std::string movesStr = g.getValueOfMoves();
        if (reader->parse(movesStr.c_str(), movesStr.c_str() + movesStr.size(),
                          &movesJson, &errs)) {
          ret["moves"] = movesJson;
        }
        callback(drogon::HttpResponse::newHttpJsonResponse(ret));
      },
      [callback](const drogon::orm::DrogonDbException &e) {
        Json::Value err;
        err["error"] = "Game not found";
        callback(drogon::HttpResponse::newHttpJsonResponse(err));
      });
}