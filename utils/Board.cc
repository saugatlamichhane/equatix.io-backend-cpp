#include "Board.h"

// Initialize ASCII lookup tables at compile/link phase to achieve O(1) direct
// character referencing
namespace {
consteval std::array<int, 128> generateTileCounts() {
  std::array<int, 128> counts{};
  // Map individual character values directly to their inventory limit
  counts['0'] = 1;
  counts['1'] = 1;
  counts['2'] = 1;
  counts['3'] = 1;
  counts['4'] = 1;
  counts['5'] = 1;
  counts['6'] = 1;
  counts['7'] = 1;
  counts['8'] = 1;
  counts['9'] = 1;
  counts['-'] = 2;
  counts['+'] = 2;
  counts['/'] = 2;
  counts['*'] = 2;
  counts['='] = 8;
  return counts;
}

consteval std::array<int, 128> generateTilePoints() {
  std::array<int, 128> points{};
  constexpr char targetSymbols[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                                    '8', '9', '-', '+', '/', '*', '='};
  for (char c : targetSymbols) {
    points[static_cast<std::size_t>(c)] = 5;
  }
  return points;
}
} // namespace

extern const std::array<int, 128> kTileCount = generateTileCounts();
extern const std::array<int, 128> kTilePoints = generateTilePoints();

[[nodiscard]] int
scoreEquation(std::span<const Json::Value> equation,
              std::span<const std::pair<int, int>> newlyPlaced) noexcept {
  int wordMultiplier = 1;
  int score = 0;

  for (const auto &tile : equation) {
    const int r = tile["row"].asInt();
    const int c = tile["col"].asInt();

    // Use string_view to point straight into the existing JSON buffer without
    // dynamic allocations
    const std::string_view val = tile["value"].asCString();
    if (val.empty()) [[unlikely]]
      continue;

    // Perform O(1) zero-overhead table indexing using the character primitive
    const char tileChar = val[0];
    int letterScore = kTilePoints[static_cast<std::size_t>(tileChar)];

    // Check if the coordinate position exists inside the newlyPlaced span
    bool isNewlyPlaced = false;
    for (const auto &[placedRow, placedCol] : newlyPlaced) {
      if (placedRow == r && placedCol == c) {
        isNewlyPlaced = true;
        break;
      }
    }

    if (isNewlyPlaced) {
      // Fetch multiplier directly from the flat compile-time constant array
      // layout
      switch (BoardConfig::kBoardMultipliers[r][c]) {
      case MultiplierType::DOUBLE_TILE:
        letterScore *= 2;
        break;
      case MultiplierType::TRIPLE_TILE:
        letterScore *= 3;
        break;
      case MultiplierType::DOUBLE_EQUATION:
        wordMultiplier *= 2;
        break;
      case MultiplierType::TRIPLE_EQUATION:
        wordMultiplier *= 3;
        break;
      default:
        break;
      }
    }
    score += letterScore;
  }
  return score * wordMultiplier;
}