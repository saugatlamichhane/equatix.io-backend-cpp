#pragma once

#include <array>
#include <json/json.h>
#include <span>
#include <string_view>

enum class MultiplierType : uint8_t {
  NONE,
  DOUBLE_TILE,
  TRIPLE_TILE,
  DOUBLE_EQUATION,
  TRIPLE_EQUATION
};

// Guarantee at compile time that our enum takes exactly 1 byte to optimize
// cache-line footprint
static_assert(sizeof(MultiplierType) == 1,
              "MultiplierType must be optimized to 1 byte");

namespace BoardConfig {
inline constexpr std::size_t BOARD_SIZE = 15;
using BoardMatrix =
    std::array<std::array<MultiplierType, BOARD_SIZE + 1>, BOARD_SIZE + 1>;

// Helper compile-time structure to replace std::pair inside consteval engines
struct Coord {
  std::size_t row;
  std::size_t col;
};

// Compile-time factory that computes the entire board state configuration once
// during compilation
consteval BoardMatrix generateBoardMultipliers() {
  BoardMatrix matrix{}; // Default initializations match MultiplierType::NONE

  // Triple Equation Coordinates (1-indexed matching your game rules)
  constexpr std::array<Coord, 8> tripleEquations = {
      Coord{1, 1}, {1, 8},  {1, 15}, {8, 1},
      {8, 15},     {15, 1}, {15, 8}, {15, 15}};
  for (const auto &pos : tripleEquations)
    matrix[pos.row][pos.col] = MultiplierType::TRIPLE_EQUATION;

  // Double Equation Coordinates
  constexpr std::array<Coord, 17> doubleEquations = {
      Coord{2, 2}, {2, 14}, {3, 3},   {3, 13}, {4, 4},   {4, 12},
      {5, 5},      {5, 11}, {8, 8},   {11, 5}, {11, 11}, {12, 4},
      {12, 12},    {13, 3}, {13, 13}, {14, 2}, {14, 14}};
  for (const auto &pos : doubleEquations)
    matrix[pos.row][pos.col] = MultiplierType::DOUBLE_EQUATION;

  // Triple Tile Coordinates
  constexpr std::array<Coord, 12> tripleTiles = {
      Coord{2, 6}, {2, 10}, {6, 2},   {6, 6},   {6, 10}, {6, 14},
      {10, 2},     {10, 6}, {10, 10}, {10, 14}, {14, 6}, {14, 10}};
  for (const auto &pos : tripleTiles)
    matrix[pos.row][pos.col] = MultiplierType::TRIPLE_TILE;

  // Double Tile Coordinates
  constexpr std::array<Coord, 24> doubleTiles = {
      Coord{1, 4}, {1, 12},  {3, 7},  {3, 9},  {4, 1},  {4, 8},
      {4, 15},     {7, 3},   {7, 7},  {7, 9},  {7, 13}, {8, 4},
      {8, 12},     {9, 3},   {9, 7},  {9, 9},  {9, 13}, {12, 1},
      {12, 8},     {12, 15}, {13, 7}, {13, 9}, {15, 4}, {15, 12}};
  for (const auto &pos : doubleTiles)
    matrix[pos.row][pos.col] = MultiplierType::DOUBLE_TILE;

  return matrix;
}

// High-performance global compile-time constant lookup matrix
inline constexpr BoardMatrix kBoardMultipliers = generateBoardMultipliers();
} // namespace BoardConfig

// Low-latency global lookup tables for tiles replacing complex map
// infrastructures
extern const std::array<int, 128> kTileCount;
extern const std::array<int, 128> kTilePoints;

// Optimized function signature accepting zero-overhead, non-owning span views
// over objects
[[nodiscard]] int
scoreEquation(std::span<const Json::Value> equation,
              std::span<const std::pair<int, int>> newlyPlaced) noexcept;