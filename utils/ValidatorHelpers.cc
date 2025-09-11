#include "ValidatorHelpers.h"
#include <set>
#include <utility>
#include <algorithm>
#include <trantor/utils/Logger.h>
#include <sstream>
#include <stdexcept>

bool touchesCenter(const std::vector<Json::Value> &current) {
  for (auto &t : current) {
    if (t["row"].asInt() == 8 && t["col"].asInt() == 8) {
      return true;
    }
  }
  return false;
}

bool touchesExisting(const std::vector<Json::Value> &state,
                     const std::vector<Json::Value> &current) {
  std::set<std::pair<int, int>> placed;
  for (auto &t : current) {

    placed.insert({t["row"].asInt(), t["col"].asInt()});
  }
  for (auto &t : current) {
    int r = t["row"].asInt();
    int c = t["col"].asInt();
    std::vector<std::pair<int, int>> neighbors = {
        {r - 1, c}, {r + 1, c}, {r, c - 1}, {r, c + 1}};
    for (auto &nb : neighbors) {
      for (auto &s : state) {
        if (s["row"].asInt() == nb.first && s["col"].asInt() == nb.second) {
          return true;
        }
      }
    }
  }
  return false;
}



bool isOccupied(std::vector<Json::Value> current_,
                std::vector<Json::Value> state_, int row, int col) {
  for (auto &item : current_) {
    if (item["row"].asInt() == row && item["col"].asInt() == col) {
      return true;
    }
  }
  for (auto &item : state_) {
    if (item["row"].asInt() == row && item["col"].asInt() == col) {
      return true;
    }
  }
  return false;
}

bool isStraightLine(std::vector<Json::Value> current_, int row, int col) {
  if (current_.empty())
    return true;
  bool isHorizontal = true;
  int firstRow = current_.at(0)["row"].asInt();
  for (const auto &tile : current_) {
    if (tile["row"].asInt() != firstRow) {
      isHorizontal = false;
      break;
    }
  }
  if (row != firstRow)
    isHorizontal = false;

  bool isVertical = true;
  int firstCol = current_[0]["col"].asInt();

  for (const auto &tile : current_) {
    if (tile["col"].asInt() != firstCol) {
      isVertical = false;
      break;
    }
  }
  if (col != firstCol)
    isVertical = false;
  return isHorizontal || isVertical;
}

bool isContiguous(std::vector<Json::Value> state,
                  std::vector<Json::Value> current, int row, int col) {
  if (current.empty())
    return true;
  bool sameRow =
      std::all_of(current.begin(), current.end(),
                  [&](const auto &tile) { return tile["row"].asInt() == row; });
  bool sameCol =
      std::all_of(current.begin(), current.end(),
                  [&](const auto &tile) { return tile["col"].asInt() == col; });
  if (!sameRow && !sameCol)
    return false;

  if (sameRow) {
    std::set<int> cols;
    for (const auto &tile : current) {
      if (tile["row"].asInt() == row)
        cols.insert(tile["col"].asInt());
    }
    cols.insert(col);
    for (const auto &tile : state) {
      if (tile["row"].asInt() == row)
        cols.insert(tile["col"].asInt());
    }
    int minCol = 20, maxCol = -1;
    for (const auto &tile : current) {
      minCol = std::min(minCol, tile["col"].asInt());
      maxCol = std::max(maxCol, tile["col"].asInt());
    }
    minCol = std::min(minCol, col);
    maxCol = std::max(maxCol, col);
    for (int c = minCol; c <= maxCol; ++c) {
      if (cols.find(c) == cols.end()) {
        return false;
      }
    }
  } else if (sameCol) {
    std::set<int> rows;
    for (const auto &tile : current) {
      if (tile["col"].asInt() == col)
        rows.insert(tile["row"].asInt());
    }
    rows.insert(row);
    for (const auto &tile : state) {
      if (tile["col"].asInt() == col)
        rows.insert(tile["row"].asInt());
    }
    int minRow = 20, maxRow = -1;
    for (const auto &tile : current) {
      minRow = std::min(minRow, tile["row"].asInt());
      maxRow = std::max(maxRow, tile["row"].asInt());
    }
    minRow = std::min(minRow, row);
    maxRow = std::max(maxRow, row);
    for (int r = minRow; r <= maxRow; ++r) {
      if (rows.find(r) == rows.end()) {
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
getAffectedEquations(const std::vector<Json::Value> &state,
                     const std::vector<Json::Value> &current) {
  using Coord = std::pair<int, int>;
  std::map<Coord, Json::Value> board;
  auto addTiles = [&](const std::vector<Json::Value> &tiles) {
    for (auto &t : tiles) {
      int r = t["row"].asInt();
      int c = t["col"].asInt();
      board[{r, c}] = t;
    }
  };
  addTiles(state);
  addTiles(current);
  std::vector<std::vector<Json::Value>> affected;
  std::set<std::string> seen;

  for (auto &t : current) {
    int r = t["row"].asInt();
    int c = t["col"].asInt();

    std::vector<Json::Value> rowSeq;

    int cc = c;
    while (board.count({r, cc}))
      cc--;

    cc++;

    while (board.count({r, cc})) {
      rowSeq.push_back(board[{r, cc}]);
      cc++;
    }
    if (rowSeq.size() > 1) {
      std::string key = "R:" + std::to_string(r) + ":" +
                        std::to_string(rowSeq.front()["col"].asInt()) + "-" +
                        std::to_string(rowSeq.back()["col"].asInt());
      if (!seen.count(key)) {
        seen.insert(key);
        affected.push_back(rowSeq);
      }
    }
    std::vector<Json::Value> colSeq;
    int rr = r;
    while (board.count({rr, c})) {
      rr--;
    }
    rr++;
    while (board.count({rr, c})) {
      colSeq.push_back(board[{rr, c}]);
      rr++;
    }

    if (colSeq.size() > 1) {
      std::string key = "C:" + std::to_string(c) + ":" +
                        std::to_string(colSeq.front()["row"].asInt()) + "-" +
                        std::to_string(colSeq.back()["row"].asInt());
      if (!seen.count(key)) {
        seen.insert(key);
        affected.push_back(colSeq);
      }
    }
  }
  return affected;
}

int precedence(char op) {
  if (op == '+' || op == '-')
    return 1;
  if (op == '*' || op == '/')
    return 2;
}

int applyOp(int a, int b, char op) {
  switch (op) {
  case '+':
    return a + b;
  case '-':
    return a - b;
  case '*':
    return a * b;
  case '/':
    if (b == 0)
      throw std::runtime_error("Division by zero");
    if (a % b != 0)
      throw std::runtime_error("Not divisible");
    return a / b;
  }
  throw std::runtime_error("invalid operator");
}

int evaluateExpression(const std::string &expr) {
  std::stack<int> values;
  std::stack<char> ops;

  size_t i = 0;
  while (i < expr.size()) {
    if (isdigit(expr[i])) {
      int val = 0;
      while (i < expr.size() && isdigit(expr[i])) {
        val = val * 10 + (expr[i] - '0');
        i++;
      }
      values.push(val);
    } else if (expr[i] == '+' || expr[i] == '-' || expr[i] == '*' ||
               expr[i] == '/') {
      while (!ops.empty() && precedence(ops.top()) >= precedence(expr[i])) {
        int b = values.top();
        values.pop();
        int a = values.top();
        values.pop();
        char op = ops.top();
        ops.pop();
        values.push(applyOp(a, b, op));
      }
      ops.push(expr[i]);
      i++;
    } else {
      throw std::runtime_error("Invalid character in expression");
    }
  }
  while (!ops.empty()) {
    int b = values.top();
    values.pop();
    int a = values.top();
    values.pop();
    char op = ops.top();
    ops.pop();
    values.push(applyOp(a, b, op));
  }
  return values.top();
}

std::vector<std::string> splitEquation(const std::string &eq) {
  std::vector<std::string> parts;
  std::stringstream ss(eq);
  std::string token;
  while (std::getline(ss, token, '=')) {
    parts.push_back(token);
  }
  return parts;
}