#include "ValidatorHelpers.h"
#include <algorithm>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <sstream>
#include <stack>
#include <trantor/utils/Logger.h>
#include <utility>

bool touchesCenter(std::span<const Json::Value> current) noexcept {
  return std::ranges::any_of(current, [](const Json::Value &t) {
    return t["row"].asInt() == 8 && t["col"].asInt() == 8;
  });
}

bool touchesExisting(std::span<const Json::Value> state,
                     std::span<const Json::Value> current) noexcept {
  for (const auto &t : current) {
    const int r = t["row"].asInt();
    const int c = t["col"].asInt();
    const bool adjacent =
        std::ranges::any_of(state, [r, c](const Json::Value &s) {
          const int sr = s["row"].asInt();
          const int sc = s["col"].asInt();
          return (std::abs(sr - r) == 1 && sc == c) ||
                 (std::abs(sc - c) == 1 && sr == r);
        });
    if (adjacent)
      return true;
  }
  return false;
}

bool isOccupied(std::span<const Json::Value> current_,
                std::span<const Json::Value> state_, int row,
                int col) noexcept {
  const auto matches = [row, col](const Json::Value &item) {
    return item["row"].asInt() == row && item["col"].asInt() == col;
  };
  return std::ranges::any_of(current_, matches) ||
         std::ranges::any_of(state_, matches);
}

bool isStraightLine(std::span<const Json::Value> current_, int row,
                    int col) noexcept {
  if (current_.empty())
    return true;
  const int firstRow = current_[0]["row"].asInt();
  const int firstCol = current_[0]["col"].asInt();
  const bool isHorizontal =
      (row == firstRow) &&
      std::ranges::all_of(current_, [firstRow](const Json::Value &t) {
        return t["row"].asInt() == firstRow;
      });
  const bool isVertical =
      (col == firstCol) &&
      std::ranges::all_of(current_, [firstCol](const Json::Value &t) {
        return t["col"].asInt() == firstCol;
      });
  return isHorizontal || isVertical;
}

bool isContiguous(std::span<const Json::Value> state,
                  std::span<const Json::Value> current, int row, int col) {
  if (current.empty())
    return true;
  const bool sameRow = std::ranges::all_of(
      current, [row](const Json::Value &t) { return t["row"].asInt() == row; });
  const bool sameCol = std::ranges::all_of(
      current, [col](const Json::Value &t) { return t["col"].asInt() == col; });
  if (!sameRow && !sameCol)
    return false;

  if (sameRow) {
    std::set<int> cols;
    for (const auto &t : current)
      cols.insert(t["col"].asInt());
    cols.insert(col);
    for (const auto &t : state)
      if (t["row"].asInt() == row)
        cols.insert(t["col"].asInt());

    int minCol = col, maxCol = col;
    for (const auto &t : current) {
      minCol = std::min(minCol, t["col"].asInt());
      maxCol = std::max(maxCol, t["col"].asInt());
    }
    for (int c = minCol; c <= maxCol; ++c) {
      if (!cols.contains(c))
        return false;
    }
  } else {
    std::set<int> rows;
    for (const auto &t : current)
      rows.insert(t["row"].asInt());
    rows.insert(row);
    for (const auto &t : state)
      if (t["col"].asInt() == col)
        rows.insert(t["row"].asInt());

    int minRow = row, maxRow = row;
    for (const auto &t : current) {
      minRow = std::min(minRow, t["row"].asInt());
      maxRow = std::max(maxRow, t["row"].asInt());
    }
    for (int r = minRow; r <= maxRow; ++r) {
      if (!rows.contains(r)) {
        LOG_DEBUG << "row " << r << " not found";
        return false;
      } else {
        LOG_DEBUG << "row " << r << " found";
      }
    }
  }
  return true;
}

std::vector<std::vector<Json::Value>>
getAffectedEquations(std::span<const Json::Value> state,
                     std::span<const Json::Value> current) {
  using Coord = std::pair<int, int>;
  std::map<Coord, Json::Value> board;
  for (const auto &t : state)
    board[{t["row"].asInt(), t["col"].asInt()}] = t;
  for (const auto &t : current)
    board[{t["row"].asInt(), t["col"].asInt()}] = t;

  std::vector<std::vector<Json::Value>> affected;
  std::set<std::string> seen;

  for (const auto &t : current) {
    const int r = t["row"].asInt();
    const int c = t["col"].asInt();

    // Scan the horizontal run containing (r, c)
    int cc = c;
    while (board.count({r, cc}))
      --cc;
    ++cc;
    std::vector<Json::Value> rowSeq;
    while (board.count({r, cc}))
      rowSeq.push_back(board[{r, cc++}]);
    if (rowSeq.size() > 1) {
      const std::string key = "R:" + std::to_string(r) + ":" +
                              std::to_string(rowSeq.front()["col"].asInt()) +
                              "-" +
                              std::to_string(rowSeq.back()["col"].asInt());
      if (!seen.contains(key)) {
        seen.insert(key);
        affected.push_back(std::move(rowSeq));
      }
    }

    // Scan the vertical run containing (r, c)
    int rr = r;
    while (board.count({rr, c}))
      --rr;
    ++rr;
    std::vector<Json::Value> colSeq;
    while (board.count({rr, c}))
      colSeq.push_back(board[{rr++, c}]);
    if (colSeq.size() > 1) {
      const std::string key = "C:" + std::to_string(c) + ":" +
                              std::to_string(colSeq.front()["row"].asInt()) +
                              "-" +
                              std::to_string(colSeq.back()["row"].asInt());
      if (!seen.contains(key)) {
        seen.insert(key);
        affected.push_back(std::move(colSeq));
      }
    }
  }
  return affected;
}

std::optional<int> applyOp(int a, int b, char op) noexcept {
  switch (op) {
  case '+':
    return a + b;
  case '-':
    return a - b;
  case '*':
    return a * b;
  case '/':
    if (b == 0 || a % b != 0)
      return std::nullopt;
    return a / b;
  default:
    return std::nullopt;
  }
}

std::optional<int> evaluateExpression(std::string_view expr) noexcept {
  std::stack<int> values;
  std::stack<char> ops;

  for (std::size_t i = 0; i < expr.size();) {
    if (std::isdigit(static_cast<unsigned char>(expr[i]))) {
      int val = 0;
      while (i < expr.size() &&
             std::isdigit(static_cast<unsigned char>(expr[i])))
        val = val * 10 + (expr[i++] - '0');
      values.push(val);
    } else if (expr[i] == '+' || expr[i] == '-' || expr[i] == '*' ||
               expr[i] == '/') {
      while (!ops.empty() && precedence(ops.top()) >= precedence(expr[i])) {
        if (values.size() < 2)
          return std::nullopt;
        const int b = values.top();
        values.pop();
        const int a = values.top();
        values.pop();
        const char op = ops.top();
        ops.pop();
        const auto res = applyOp(a, b, op);
        if (!res)
          return std::nullopt;
        values.push(*res);
      }
      ops.push(expr[i++]);
    } else {
      return std::nullopt;
    }
  }
  while (!ops.empty()) {
    if (values.size() < 2)
      return std::nullopt;
    const int b = values.top();
    values.pop();
    const int a = values.top();
    values.pop();
    const char op = ops.top();
    ops.pop();
    const auto res = applyOp(a, b, op);
    if (!res)
      return std::nullopt;
    values.push(*res);
  }
  if (values.empty())
    return std::nullopt;
  return values.top();
}

std::vector<std::string> splitEquation(std::string_view eq) {
  std::vector<std::string> parts;
  std::size_t pos = 0;
  std::size_t found;
  while ((found = eq.find('=', pos)) != std::string_view::npos) {
    parts.emplace_back(eq.substr(pos, found - pos));
    pos = found + 1;
  }
  parts.emplace_back(eq.substr(pos));
  return parts;
}
