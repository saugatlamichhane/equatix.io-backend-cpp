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



void PuzzleController::listPuzzles(
    const HttpRequestPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // 1. Extract UID from attributes (set by FirebaseAuthFilter)
    auto uidAttr = req->attributes()->get<std::string>("uid");
    if (uidAttr.empty())
    {
        Json::Value err;
        err["error"] = "Missing UID";
        callback(HttpResponse::newHttpJsonResponse(err));
        return;
    }
    std::string userId = uidAttr;

    // 2. DB client
    auto db = app().getDbClient();

    // 3. Query puzzles + user progress in one SQL (LEFT JOIN)
    db->execSqlAsync(
        "SELECT p.puzzle_id, p.difficulty, p.objective, "
        "       COALESCE(pp.solved, FALSE) AS solved "
        "FROM puzzles p "
        "LEFT JOIN puzzle_progress pp "
        "  ON pp.puzzle_id = p.puzzle_id AND pp.user_id = $1 "
        "ORDER BY p.puzzle_id ASC",
        [callback](const Result &r) {
            Json::Value list(Json::arrayValue);

            for (auto const &row : r)
            {
                Json::Value item;
                item["puzzle_id"] = row["puzzle_id"].as<int>();
                item["difficulty"] = row["difficulty"].as<std::string>();
                item["objective"] = row["objective"].as<std::string>();
                item["solved"] = row["solved"].as<bool>();

                list.append(item);
            }

            auto resp = HttpResponse::newHttpJsonResponse(list);
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
        userId  // SQL parameter $1
    );
}

