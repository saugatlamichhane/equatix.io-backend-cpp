#pragma once
#include "RoomState.h"
#include <string>
#include <vector>
#include <json/json.h>

struct BotMove {
    bool isValid = false;
    std::vector<Json::Value> placedTiles;
    std::vector<std::string> usedValues;
    int score = 0;
};

// Existing helpers
void reset(RoomState &room, int playerTurn);
std::vector<std::string> createTileBag();
std::vector<std::string> drawTiles(std::vector<std::string> &tileBag, int n);

inline auto checkEndGame = [](RoomState &room) {
    return (room.tileBag.empty() && (room.player1Rack.empty() || room.player2Rack.empty())) || room.passes >= 6;
};

// AI Bot Logic
namespace GameLogic {
    BotMove findBestMove(const std::vector<Json::Value>& boardState, const std::vector<std::string>& rack);
    BotMove searchFirstMove(const std::vector<std::string>& rack);
    BotMove findHighestScoreAtPos(int r, int c, bool horizontal, const std::vector<Json::Value>& board, const std::vector<std::string>& rack);
    
    // Internal algorithm helpers
    std::vector<Json::Value> convertToTiles(const std::vector<std::string>& expr, int r, int c, bool horizontal);
    bool isAdjacentToTile(const std::vector<Json::Value>& board, int r, int c);
}