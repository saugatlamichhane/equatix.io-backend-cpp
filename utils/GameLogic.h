#pragma once
#include "RoomState.h"
#include <json/json.h>
#include <optional>
#include <span>
#include <string>
#include <vector>

struct BotMove {
  std::vector<Json::Value> placedTiles;
  std::vector<std::string> usedValues;
  int score = 0;
};

void reset(RoomState &room, int playerTurn);
[[nodiscard]] std::vector<std::string> createTileBag();
[[nodiscard]] std::vector<std::string>
drawTiles(std::vector<std::string> &tileBag, int n);

inline auto checkEndGame = [](const RoomState &room) noexcept {
  return (room.tileBag.empty() &&
          (room.player1Rack.empty() || room.player2Rack.empty())) ||
         room.passes >= 6;
};

namespace GameLogic {
// Returns nullopt when no valid move exists
[[nodiscard]] std::optional<BotMove>
findBestMove(std::span<const Json::Value> boardState,
             std::span<const std::string> rack);

[[nodiscard]] std::optional<BotMove>
searchFirstMove(std::span<const std::string> rack);

[[nodiscard]] std::optional<BotMove>
findHighestScoreAtPos(int r, int c, bool horizontal,
                      std::span<const Json::Value> board,
                      std::span<const std::string> rack);

[[nodiscard]] std::vector<Json::Value>
convertToTiles(std::span<const std::string> expr, int r, int c,
               bool horizontal);

[[nodiscard]] bool isAdjacentToTile(std::span<const Json::Value> board, int r,
                                    int c) noexcept;
} // namespace GameLogic
