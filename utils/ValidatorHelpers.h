#pragma once

#include <json/json.h>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

[[nodiscard]] bool touchesCenter(std::span<const Json::Value> current) noexcept;
[[nodiscard]] bool
touchesExisting(std::span<const Json::Value> state,
                std::span<const Json::Value> current) noexcept;
[[nodiscard]] bool isOccupied(std::span<const Json::Value> current_,
                              std::span<const Json::Value> state_, int row,
                              int col) noexcept;
[[nodiscard]] bool isStraightLine(std::span<const Json::Value> current_,
                                  int row, int col) noexcept;
[[nodiscard]] bool isContiguous(std::span<const Json::Value> state,
                                std::span<const Json::Value> current, int row,
                                int col);
[[nodiscard]] std::vector<std::vector<Json::Value>>
getAffectedEquations(std::span<const Json::Value> state,
                     std::span<const Json::Value> current);

// Inline constexpr: callable at compile time, fixes missing-return UB in
// original
[[nodiscard]] inline constexpr int precedence(char op) noexcept {
  if (op == '+' || op == '-')
    return 1;
  if (op == '*' || op == '/')
    return 2;
  return 0;
}

[[nodiscard]] std::optional<int> applyOp(int a, int b, char op) noexcept;
[[nodiscard]] std::optional<int>
evaluateExpression(std::string_view expr) noexcept;
[[nodiscard]] std::vector<std::string> splitEquation(std::string_view eq);
