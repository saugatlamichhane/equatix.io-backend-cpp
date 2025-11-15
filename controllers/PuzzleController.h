#pragma once
#include <drogon/HttpController.h>

using namespace drogon;

class PuzzleController : public drogon::HttpController<PuzzleController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(PuzzleController::getPuzzle, "/puzzle/{1}", Get,
                "FirebaseAuthFilter");
  ADD_METHOD_TO(PuzzleController::listPuzzles, "/puzzles", Get,
                "FirebaseAuthFilter");
  ADD_METHOD_TO(PuzzleController::validateMove, "/puzzle/validateMove", Post,
                "FirebaseAuthFilter");
  METHOD_LIST_END

  void getPuzzle(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback,
                 int puzzleId);
  void listPuzzles(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
  void validateMove(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
};
