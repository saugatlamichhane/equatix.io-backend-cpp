#include "PuzzleController.h"
#include <drogon/drogon.h>
#include <drogon/orm/Mapper.h>
#include <json/json.h>

using namespace drogon;
using namespace drogon::orm;

void PuzzleController::getPuzzle(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback,
    int puzzleId) 
{
    auto db = app().getDbClient();

    db->execSqlAsync(
        "SELECT puzzle_id, board_state, rack, objective, difficulty "
        "FROM puzzles WHERE puzzle_id=$1",
        [callback](const Result &r) {
            if (r.empty()) {
                auto resp = HttpResponse::newHttpJsonResponse(
                    Json::Value("Puzzle not found")
                );
                resp->setStatusCode(k404NotFound);
                callback(resp);
                return;
            }

            Json::Value res;
            Json::Value boardJson;
            Json::Value rackJson;

            // Parse stored JSON strings
            Json::CharReaderBuilder builder;
            std::string errs;

            std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
            std::string boardStr = r[0]["board_state"].as<std::string>();
            reader->parse(boardStr.c_str(), boardStr.c_str() + boardStr.size(), &boardJson, &errs);

            reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
            std::string rackStr = r[0]["rack"].as<std::string>();
            reader->parse(rackStr.c_str(), rackStr.c_str() + rackStr.size(), &rackJson, &errs);

            res["puzzle_id"] = r[0]["puzzle_id"].as<int>();
            res["difficulty"] = r[0]["difficulty"].as<std::string>();
            res["objective"] = r[0]["objective"].as<std::string>();
            res["board"] = boardJson;   // Already formatted as [{"row":...}]
            res["rack"] = rackJson;     // ["1","2","+"]

            auto resp = HttpResponse::newHttpJsonResponse(res);
            resp->setStatusCode(k200OK);
            callback(resp);
        },
        [callback](const DrogonDbException &e) {
            Json::Value err;
            err["error"] = e.base().what();
            auto resp = HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        puzzleId
    );
}
