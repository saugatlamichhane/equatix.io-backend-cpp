#pragma once
#include <drogon/HttpController.h>

using namespace drogon;

class ChallengeController : public drogon::HttpController<ChallengeController> {
public:
  METHOD_LIST_BEGIN
  ADD_METHOD_TO(ChallengeController::sendChallenge,
                "/challenge/send/{opponentId}", Post, "FirebaseAuthFilter");
  ADD_METHOD_TO(ChallengeController::acceptChallenge,
                "/challenge/accept/{challengeId}", Put, "FirebaseAuthFilter");
  ADD_METHOD_TO(ChallengeController::rejectChallenge,
                "/challenge/reject/{challengeId}", Put, "FirebaseAuthFilter");
  ADD_METHOD_TO(ChallengeController::listSentChallenges, "/challenge/sent", Get,
                "FirebaseAuthFilter");
  ADD_METHOD_TO(ChallengeController::listReceivedChallenges,
                "/challenge/received", Get, "FirebaseAuthFilter");
  ADD_METHOD_TO(ChallengeController::cancelSentChallenge,
                "/challenge/cancel/{challengeId}", Delete,
                "FirebaseAuthFilter");
  METHOD_LIST_END

  void sendChallenge(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback,
                     const std::string &opponentId);
  void acceptChallenge(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback,
                       const std::string &challengeId);
  void rejectChallenge(const HttpRequestPtr &req,
                       std::function<void(const HttpResponsePtr &)> &&callback,
                       const std::string &challengeId);
  void
  listSentChallenges(const HttpRequestPtr &req,
                     std::function<void(const HttpResponsePtr &)> &&callback);
  void listReceivedChallenges(
      const HttpRequestPtr &req,
      std::function<void(const HttpResponsePtr &)> &&callback);
  void
  cancelSentChallenge(const HttpRequestPtr &req,
                      std::function<void(const HttpResponsePtr &)> &&callback,
                      const std::string &challengeId);
};
