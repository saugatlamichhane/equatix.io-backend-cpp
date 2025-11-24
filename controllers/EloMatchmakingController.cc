#include "EloMatchmakingController.h"
#include <trantor/utils/Logger.h>
#include "LiveStatsController.h"

void EloMatchmakingController::handleNewMessage(const WebSocketConnectionPtr &wsConnPtr,
                                                std::string &&message,
                                                const WebSocketMessageType &type)
{
    if (type != WebSocketMessageType::Text)
        return;

    // Parse JSON
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

    if (!reader->parse(message.data(), message.data() + message.size(), &root, &errs)) {
        LOG_ERROR << "Invalid JSON: " << errs;
        return;
    }

    std::string userId = root["userId"].asString();
    int elo = root["elo"].asInt();

    WebSocketConnectionPtr opponentWsConn;
    std::string opponentId;

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        wsMap_[userId] = wsConnPtr;

        // Look for opponent in acceptable Elo range
        auto it = std::find_if(waitingQueue_.begin(), waitingQueue_.end(),
                               [elo, this](const std::tuple<std::string,int>& t) {
                                   return std::abs(std::get<1>(t) - elo) <= ELO_RANGE;
                               });

        if (it != waitingQueue_.end()) {
            opponentId = std::get<0>(*it);
            opponentWsConn = wsMap_[opponentId];
            waitingQueue_.erase(it);
        } else {
            waitingQueue_.emplace_back(userId, elo);
            return;  // wait
        }
    }

    // Insert challenge into database
    auto clientPtr = drogon::app().getDbClient();
    std::string sql = "INSERT INTO challenges (challenger_id, opponent_id, status) "
                      "VALUES ($1, $2, 'accepted') RETURNING id;";

    clientPtr->execSqlAsync(
        sql,
        [wsConnPtr, opponentWsConn, userId, opponentId](const drogon::orm::Result &r){
            int challengeId = r[0]["id"].as<int>();

            Json::Value resp;
            resp["message"] = "found";
            resp["challengeId"] = challengeId;
            resp["status"] = "accepted";

            // Notify user
            resp["opponentId"] = opponentId;
            wsConnPtr->send(Json::writeString(Json::StreamWriterBuilder(), resp));

            // Notify opponent
            resp["opponentId"] = userId;
            opponentWsConn->send(Json::writeString(Json::StreamWriterBuilder(), resp));
        },
        [wsConnPtr](const drogon::orm::DrogonDbException &e){
            Json::Value err;
            err["error"] = e.base().what();
            wsConnPtr->send(Json::writeString(Json::StreamWriterBuilder(), err));
        },
        userId, opponentId
    );
}

void EloMatchmakingController::handleConnectionClosed(const WebSocketConnectionPtr &wsConnPtr)
{
    // Remove closed connection from wsMap_ and waitingQueue_
    std::string toErase;
    LiveStatsController::decrementActivePlayers();

    {
        std::lock_guard<std::mutex> lock(queueMutex_);

        // Find userId for this wsConn
        for (auto it = wsMap_.begin(); it != wsMap_.end(); ++it) {
            if (it->second == wsConnPtr) {
                toErase = it->first;
                wsMap_.erase(it);
                break;
            }
        }

        if (!toErase.empty()) {
            waitingQueue_.erase(
                std::remove_if(waitingQueue_.begin(), waitingQueue_.end(),
                    [&toErase](const std::tuple<std::string,int>& t){
                        return std::get<0>(t) == toErase;
                    }),
                waitingQueue_.end()
            );
        }
    }
}

void EloMatchmakingController::handleNewConnection(const HttpRequestPtr &,
                                                   const WebSocketConnectionPtr &)
{
    // Nothing needed
    LiveStatsController::incrementActivePlayers();
}
