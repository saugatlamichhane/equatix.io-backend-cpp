#pragma once
#include <drogon/HttpController.h>

using namespace drogon;

class PuzzleController : public drogon::HttpController<PuzzleController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(PuzzleController::getPuzzle, "/puzzle/{1}", Get);
    METHOD_LIST_END

    void getPuzzle(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback,
                   int puzzleId);
};
