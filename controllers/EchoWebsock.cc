#include "EchoWebsock.h"
#include "ValidationHelpers.h"
#include "GameLogic.h"
#include <drogon/WebSocketController.h>
#include <json/json.h>
#include <trantor/utils/Logger.h>

using namespace drogon;

void EchoWebsock::handleNewMessage(
    const WebSocketConnectionPtr &wsConnPtr,
    std::string &&message,
    const WebSocketMessageType &type)
{
    try {
        Json::CharReaderBuilder rbuilder;
        Json::Value msg;
        std::string errs;
        std::istringstream s(message);
        if (!Json::parseFromStream(rbuilder, s, &msg, &errs)) {
            LOG_ERROR << "Failed to parse message: " << errs;
            return;
        }

        auto roomId = msg["roomId"].asString();
        auto &room = rooms[roomId]; // RoomState stored globally or in a map

        if (msg["type"].asString() == "join") {
            if (!room.player1Conn) {
                room.player1Conn = wsConnPtr;
            } else if (!room.player2Conn) {
                room.player2Conn = wsConnPtr;
            }
            Json::Value resp;
            resp["type"] = "joined";
            resp["playerNum"] = (!room.player1Conn ? 1 : 2);
            wsConnPtr->send(Json::writeString(Json::StreamWriterBuilder(), resp));
        } else if (msg["type"].asString() == "placeTile") {
            Json::Value tile = msg["tile"];
            int row = tile["row"].asInt();
            int col = tile["col"].asInt();
            std::string value = tile["value"].asString();

            if (isOccupied(room.state_, room.current_, row, col)) {
                Json::Value errMsg;
                errMsg["type"] = "error";
                errMsg["message"] = "Tile position already occupied";
                wsConnPtr->send(Json::writeString(Json::StreamWriterBuilder(), errMsg));
                return;
            }

            if (!isStraightLine(room.current_, row, col) ||
                !isContiguous(room.state_, room.current_, row, col)) {
                Json::Value errMsg;
                errMsg["type"] = "error";
                errMsg["message"] = "Invalid tile placement";
                wsConnPtr->send(Json::writeString(Json::StreamWriterBuilder(), errMsg));
                return;
            }

            room.current_.push_back(tile);

            // Notify both players
            Json::Value update;
            update["type"] = "tilePlaced";
            update["tile"] = tile;
            if (room.player1Conn) room.player1Conn->send(Json::writeString(Json::StreamWriterBuilder(), update));
            if (room.player2Conn) room.player2Conn->send(Json::writeString(Json::StreamWriterBuilder(), update));

            // Check if turn is over
            if (room.current_.size() >= msg["tilesNeeded"].asInt()) {
                auto affectedEquations = getAffectedEquations(room.state_, room.current_);
                int score = 0;
                for (auto &eq : affectedEquations) {
                    try {
                        std::string expr = "";
                        for (auto &tile : eq) expr += tile["value"].asString();
                        auto sides = splitEquation(expr);
                        if (evaluateExpression(sides[0]) == evaluateExpression(sides[1])) {
                            score += eq.size(); // or any scoring rule
                        }
                    } catch (...) { /* ignore invalid expressions */ }
                }

                if (msg["playerNum"].asInt() == 1) room.player1Score += score;
                else room.player2Score += score;

                // Commit tiles
                for (auto &t : room.current_) room.state_.push_back(t);
                room.current_.clear();

                // Send scores
                Json::Value scoreUpdate;
                scoreUpdate["type"] = "scoreUpdate";
                scoreUpdate["player1Score"] = room.player1Score;
                scoreUpdate["player2Score"] = room.player2Score;
                if (room.player1Conn) room.player1Conn->send(Json::writeString(Json::StreamWriterBuilder(), scoreUpdate));
                if (room.player2Conn) room.player2Conn->send(Json::writeString(Json::StreamWriterBuilder(), scoreUpdate));

                // Reset for next turn
                reset(room, msg["playerNum"].asInt());
            }
        }
    } catch (const std::exception &e) {
        LOG_ERROR << "Exception in handleNewMessage: " << e.what();
    }
}

void EchoWebsock::handleConnectionClosed(const WebSocketConnectionPtr &wsConnPtr) {
    LOG_INFO << "Connection closed";
    // Optional: clean up rooms
} 

void EchoWebsock::handleNewConnection(
    const HttpRequestPtr &req,
    const WebSocketConnectionPtr &wsConnPtr)
{
    LOG_DEBUG << "New websocket connection!";

    Subscriber s;
    s.chatRoomName_ = req->getParameter("room_name");
    auto &room = rooms[s.chatRoomName_];

    // Check for bot parameter
    auto params = req->getParameters();
    room.isBot = (params.find("isBot") != params.end() && params["isBot"] == "1");

    // Initialize response
    Json::Value init;
    init["type"] = "init";
    init["turn"] = room.currentTurn;
    init["roomName"] = s.chatRoomName_;

    // Assign players
    if (!room.player1Conn) {
        room.player1Conn = wsConnPtr;
        room.tileBag = createTileBag();
        room.player1Rack = drawTiles(room.tileBag, 8);
        for (auto &tile : room.player1Rack) init["rack"].append(tile);

        if (room.isBot && !room.player2Conn) {
            room.player2Conn = nullptr;  // no actual connection
            room.player2Rack = drawTiles(room.tileBag, 8);
        }
        init["sent"] = 1;

    } else if (!room.isBot && !room.player2Conn) {
        room.player2Conn = wsConnPtr;
        room.player2Rack = drawTiles(room.tileBag, 8);
        for (auto &tile : room.player2Rack) init["rack"].append(tile);
        init["sent"] = 2;

    } else {
        init["error"] = "2 Players already connected";
        wsConnPtr->send(Json::writeString(Json::StreamWriterBuilder(), init));
        wsConnPtr->forceClose();
        return;
    }

    // Subscribe WebSocket to chatRoom
    s.id_ = chatRooms_.subscribe(
        s.chatRoomName_,
        [wsConnPtr](const std::string &, const std::string &message) {
            wsConnPtr->send(message);
        });

    wsConnPtr->setContext(std::make_shared<Subscriber>(std::move(s)));

    // Initialize board multipliers
    initBoardMultipliers();

    // Send initial state
    wsConnPtr->send(Json::writeString(Json::StreamWriterBuilder(), init));
}
