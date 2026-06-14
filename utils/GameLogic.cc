#include "GameLogic.h"
#include "Board.h"
#include "ValidatorHelpers.h"
#include <algorithm>
#include <functional>
#include <optional>
#include <random>
#include <ranges>

void reset(RoomState &room, int playerTurn) {
  std::vector<std::string> &playerRack =
      (playerTurn == 1) ? room.player1Rack : room.player2Rack;
  for (auto &tile : room.current_) {
    playerRack.push_back(tile["value"].asString());
  }
  room.current_.clear();
  Json::Value response;
  response["type"] = "reset";
  response["rack"] = Json::Value(Json::arrayValue);
  for (auto &tile : playerRack) {
    response["rack"].append(tile);
  }
  if (playerTurn == 1) {
    room.player1Conn->send(
        Json::writeString(Json::StreamWriterBuilder(), response));
  } else {
    room.player2Conn->send(
        Json::writeString(Json::StreamWriterBuilder(), response));
  }
}

std::vector<std::string> createTileBag() {
  std::vector<std::string> bag;
  constexpr char targetSymbols[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                    '8', '9', '-', '+', '/', '*', '='};
  for (char symbol : targetSymbols) {
    const int count = kTileCount[static_cast<std::size_t>(symbol)];
    for (int i = 0; i < count; ++i)
      bag.emplace_back(1, symbol);
  }
  std::mt19937 g(std::random_device{}());
  std::shuffle(bag.begin(), bag.end(), g);
  return bag;
}

std::vector<std::string> drawTiles(std::vector<std::string> &tileBag, int n) {
  if (n > static_cast<int>(tileBag.size()))
    n = static_cast<int>(tileBag.size());
  std::vector<std::string> drawnTiles(tileBag.end() - n, tileBag.end());
  tileBag.erase(tileBag.end() - n, tileBag.end());
  return drawnTiles;
}

namespace GameLogic {

bool isAdjacentToTile(std::span<const Json::Value> board, int r,
                      int c) noexcept {
  return std::ranges::any_of(board, [r, c](const Json::Value &tile) {
    const int tr = tile["row"].asInt();
    const int tc = tile["col"].asInt();
    return (std::abs(tr - r) == 1 && tc == c) ||
           (std::abs(tc - c) == 1 && tr == r);
  });
}

std::vector<Json::Value> convertToTiles(std::span<const std::string> expr,
                                        int r, int c, bool horizontal) {
  std::vector<Json::Value> tiles;
  tiles.reserve(expr.size());
  for (int i = 0; i < static_cast<int>(expr.size()); ++i) {
    Json::Value t;
    t["value"] = expr[i];
    t["row"] = horizontal ? r : r + i;
    t["col"] = horizontal ? c + i : c;
    tiles.push_back(std::move(t));
  }
  return tiles;
}

std::optional<BotMove> searchFirstMove(std::span<const std::string> rack) {
  std::optional<BotMove> best;
  constexpr int maxWordLength = 7;
  constexpr int center = 8;

  for (bool isHorizontal : {true, false}) {
    for (int i = std::max(0, center - maxWordLength + 1); i <= center; ++i) {
      const int startRow = isHorizontal ? center : i;
      const int startCol = isHorizontal ? i : center;

      auto move =
          findHighestScoreAtPos(startRow, startCol, isHorizontal, {}, rack);
      if (move) {
        const int wordLength = static_cast<int>(move->usedValues.size());
        const int endPos =
            (isHorizontal ? startCol : startRow) + wordLength - 1;
        if (endPos >= center && (!best || move->score > best->score))
          best = std::move(move);
      }
    }
  }
  return best;
}

std::optional<BotMove> findBestMove(std::span<const Json::Value> boardState,
                                    std::span<const std::string> rack) {
  if (boardState.empty())
    return searchFirstMove(rack);

  std::optional<BotMove> best;
  for (int r = 1; r <= 15; ++r) {
    for (int c = 1; c <= 15; ++c) {
      if (isOccupied(boardState, {}, r, c))
        continue;
      for (bool horizontal : {true, false}) {
        auto current =
            findHighestScoreAtPos(r, c, horizontal, boardState, rack);
        if (current && (!best || current->score > best->score))
          best = std::move(current);
      }
    }
  }
  return best;
}

std::optional<BotMove>
findHighestScoreAtPos(int r, int c, bool horizontal,
                      std::span<const Json::Value> board,
                      std::span<const std::string> rack) {
  std::optional<BotMove> best;
  std::vector<std::string> currentExpr;
  std::vector<bool> usedInRack(rack.size(), false);

  std::function<void(int, int)> backtrack = [&](int currR, int currC) {
    if (currR < 0 || currR >= 15 || currC < 0 || currC >= 15)
      return;
    if (currentExpr.size() > 7)
      return;

    std::string fullStr;
    for (const auto &s : currentExpr)
      fullStr += s;

    if (fullStr.find('=') != std::string::npos && fullStr.back() != '=' &&
        !std::ispunct(static_cast<unsigned char>(fullStr.back()))) {
      const auto parts = splitEquation(fullStr);
      if (parts.size() >= 2) {
        const auto target = evaluateExpression(parts[0]);
        if (target) {
          const bool valid =
              std::ranges::all_of(parts, [&](const std::string &p) {
                const auto val = evaluateExpression(p);
                return val.has_value() && *val == *target;
              });
          if (valid) {
            const int currentScore = static_cast<int>(fullStr.size());
            if (!best || currentScore > best->score) {
              best = BotMove{
                  .placedTiles = convertToTiles(currentExpr, r, c, horizontal),
                  .usedValues = currentExpr,
                  .score = currentScore,
              };
            }
          }
        }
      }
    }

    for (std::size_t i = 0; i < rack.size(); ++i) {
      if (!usedInRack[i]) {
        usedInRack[i] = true;
        currentExpr.push_back(std::string(rack[i]));
        horizontal ? backtrack(currR, currC + 1) : backtrack(currR + 1, currC);
        currentExpr.pop_back();
        usedInRack[i] = false;
      }
    }
  };

  backtrack(r, c);
  return best;
}

} // namespace GameLogic
