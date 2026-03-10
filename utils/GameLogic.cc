#include "GameLogic.h"
#include "Board.h"
#include "RoomState.h"
#include <algorithm>
#include <random>

#include "GameLogic.h"
#include "Board.h"
#include "ValidatorHelpers.h"
#include <algorithm>
#include <functional>

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
  for (auto &[symbol, count] : tileCount) {
    for (int i = 0; i < count; i++) {
      bag.push_back(symbol);
    }
  }
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(bag.begin(), bag.end(), g);
  return bag;
}

std::vector<std::string> drawTiles(std::vector<std::string> &tileBag, int n) {
  if (n > (int)tileBag.size()) {
    n = tileBag.size();
  }
  std::vector<std::string> drawnTiles;
  drawnTiles.insert(drawnTiles.end(), tileBag.end() - n, tileBag.end());
  tileBag.erase(tileBag.end() - n, tileBag.end());
  return drawnTiles;
}




namespace GameLogic {

bool isAdjacentToTile(const std::vector<Json::Value>& board, int r, int c) {
    for (const auto& tile : board) {
        int tr = tile["row"].asInt();
        int tc = tile["col"].asInt();
        if ((abs(tr - r) == 1 && tc == c) || (abs(tc - c) == 1 && tr == r)) return true;
    }
    return false;
}

std::vector<Json::Value> convertToTiles(const std::vector<std::string>& expr, int r, int c, bool horizontal) {
    std::vector<Json::Value> tiles;
    for (int i = 0; i < expr.size(); ++i) {
        Json::Value t;
        t["value"] = expr[i];
        t["row"] = horizontal ? r : r + i;
        t["col"] = horizontal ? c + i : c;
        tiles.push_back(t);
    }
    return tiles;
}

BotMove searchFirstMove(const std::vector<std::string>& rack) {
    BotMove bestMove{false};
    bestMove.score = -1;

    // Standard Scrabble rack size is 7
    const int maxWordLength = 7; 
    const int center = 8;

    // Check both Horizontal (true) and Vertical (false)
    for (bool isHorizontal : {true, false}) {
        
        // A word 'touches' the center if it starts at (center - length + 1) up to (center)
        // We check starting positions from (center - 6) up to (center)
        for (int i = std::max(0, center - maxWordLength + 1); i <= center; ++i) {
            
            // If horizontal, row is fixed at 8, column varies.
            // If vertical, column is fixed at 8, row varies.
            int startRow = isHorizontal ? center : i;
            int startCol = isHorizontal ? i : center;

            // Call your existing score function
            BotMove move = findHighestScoreAtPos(startRow, startCol, isHorizontal, {}, rack);

            if (move.isValid) {
                // Verification: Does this specific move actually cover (8,8)?
                // We check if startPos + number of tiles >= 8
                int wordLength = move.usedValues.size();
                int endPos = (isHorizontal ? startCol : startRow) + wordLength - 1;

                if (endPos >= center) {
                    // Track the highest scoring valid move
                    if (move.score > bestMove.score) {
                        bestMove = move;
                    }
                }
            }
        }
    }

    // If no valid move was found, bestMove.isValid remains false
    return bestMove;
}

BotMove findBestMove(const std::vector<Json::Value>& boardState, const std::vector<std::string>& rack) {
    if (boardState.empty()) return searchFirstMove(rack);

    BotMove bestMove;
    bestMove.score = -1;

    for (int r = 0; r < 15; ++r) {
        for (int c = 0; c < 15; ++c) {
            if (isOccupied(boardState, {}, r, c)) continue;

            for (bool horizontal : {true, false}) {
                BotMove current = findHighestScoreAtPos(r, c, horizontal, boardState, rack);
                if (current.isValid && current.score > bestMove.score) {
                    bestMove = current;
                }
            }
        }
    }
    return bestMove.isValid ? bestMove : BotMove{false};
}

BotMove findHighestScoreAtPos(int r, int c, bool horizontal, const std::vector<Json::Value>& board, const std::vector<std::string>& rack) {
    BotMove best;
    std::vector<std::string> currentExpr;
    std::vector<bool> usedInRack(rack.size(), false);

    std::function<void(int, int)> backtrack = [&](int currR, int currC) {
        if (currR < 0 || currR >= 15 || currC < 0 || currC >= 15) return;
        if (currentExpr.size() > 7) return; // Performance cap

        std::string fullStr = "";
        for (const auto& s : currentExpr) fullStr += s;

        // Validation logic
        if (fullStr.find('=') != std::string::npos && fullStr.back() != '=' && !ispunct(fullStr.back())) {
            auto parts = splitEquation(fullStr);
            if (parts.size() >= 2) {
                try {
                    double target = evaluateExpression(parts[0]);
                    bool valid = true;
                    for (auto& p : parts) if (evaluateExpression(p) != target) valid = false;

                    if (valid) {
                        auto placed = convertToTiles(currentExpr, r, c, horizontal);
                        // Simplified scoring (can be expanded with board multipliers)
                        int currentScore = fullStr.length(); 
                        if (currentScore > best.score) {
                            best.isValid = true;
                            best.score = currentScore;
                            best.placedTiles = placed;
                            best.usedValues = currentExpr;
                        }
                    }
                } catch (...) {}
            }
        }

        for (size_t i = 0; i < rack.size(); ++i) {
            if (!usedInRack[i]) {
                usedInRack[i] = true;
                currentExpr.push_back(rack[i]);
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