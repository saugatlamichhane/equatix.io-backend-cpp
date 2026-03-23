#pragma once
#include <drogon/HttpController.h>

using namespace drogon;

class PuzzleController : public drogon::HttpController<PuzzleController> {
public:
  METHOD_LIST_BEGIN
  // Puzzle fetching endpoints
  ADD_METHOD_TO(PuzzleController::getPuzzle, "/puzzle/{1}", Get,
                "FirebaseAuthFilter");
  ADD_METHOD_TO(PuzzleController::listPuzzles, "/puzzles", Get,
                "FirebaseAuthFilter");
  
  // Validation & submission
  ADD_METHOD_TO(PuzzleController::validateMove, "/puzzle/validateMove", Post,
                "FirebaseAuthFilter");
  ADD_METHOD_TO(PuzzleController::submitPuzzle, "/puzzle/{1}/submit", Post,
                "FirebaseAuthFilter");
  
  // Daily puzzle endpoints
  ADD_METHOD_TO(PuzzleController::getDailyPuzzle, "/puzzles/daily", Get,
                "FirebaseAuthFilter");
  ADD_METHOD_TO(PuzzleController::getStreak, "/puzzles/streak", Get,
                "FirebaseAuthFilter");
  
  // User progress/stats endpoints
  ADD_METHOD_TO(PuzzleController::getUserProgress, "/user/puzzles/progress", Get,
                "FirebaseAuthFilter");
  ADD_METHOD_TO(PuzzleController::getUserStats, "/user/stats", Get,
                "FirebaseAuthFilter");
  METHOD_LIST_END

  // Fetching endpoints
  void getPuzzle(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback,
                 int puzzleId);
  void listPuzzles(const HttpRequestPtr &req,
                   std::function<void(const HttpResponsePtr &)> &&callback);
  
  // Validation & submission
  void validateMove(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);
  void submitPuzzle(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback,
                    int puzzleId);
  
  // Daily puzzle
  void getDailyPuzzle(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback);
  void getStreak(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback);
  
  // User endpoints
  void getUserProgress(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback);
  void getUserStats(const HttpRequestPtr &req,
                    std::function<void(const HttpResponsePtr &)> &&callback);

private:
  // Helper methods (static to work with nested lambdas)
  static Json::Value createErrorResponse(const std::string &message, 
                                          const std::string &code);
  static Json::Value createSuccessResponse(const Json::Value &data);
};
