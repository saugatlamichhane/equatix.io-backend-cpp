#pragma once

#include <vector>
#include <json/json.h>
#include <string>

bool touchesCenter(const std::vector<Json::Value> &current);
bool touchesExisting(const std::vector<Json::Value> &state,
                     const std::vector<Json::Value> &current);
bool isOccupied(std::vector<Json::Value> current_,
                std::vector<Json::Value> state_, int row, int col);
bool isStraightLine(std::vector<Json::Value> current_, int row, int col);
bool isContiguous(std::vector<Json::Value> state,
                  std::vector<Json::Value> current, int row, int col);
std::vector<std::vector<Json::Value>>
getAffectedEquations(const std::vector<Json::Value> &state,
                     const std::vector<Json::Value> &current);
int precedence(char op);
int applyOp(int a, int b, char op);
int evaluateExpression(const std::string &expr);
std::vector<std::string> splitEquation(const std::string &eq);