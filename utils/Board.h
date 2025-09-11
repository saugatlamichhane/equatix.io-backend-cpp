#pragma once

#include <map>
#include <string>
#include <json/json.h>
#include <utility>
#include <vector>
#include <set>

enum class MultiplierType {
  NONE,
  DOUBLE_TILE,
  TRIPLE_TILE,
  DOUBLE_EQUATION,
  TRIPLE_EQUATION
};

std::map<std::pair<int, int>, MultiplierType> boardMultipliers;

std::map<std::string, int> tileCount{{"0", 1}, {"1", 1}, {"2", 1}, {"3", 1},
                                     {"4", 1}, {"5", 1}, {"6", 1}, {"7", 1},
                                     {"8", 1}, {"9", 1}, {"-", 2}, {"+", 2},
                                     {"/", 2}, {"*", 2}, {"=", 8}

};

std::map<std::string, int> tilePoints = {{"0", 5}, {"1", 5}, {"2", 5}, {"3", 5},
                                         {"4", 5}, {"5", 5}, {"6", 5}, {"7", 5},
                                         {"8", 5}, {"9", 5}, {"-", 5}, {"+", 5},
                                         {"/", 5}, {"*", 5}, {"=", 5}};
                                    
void initBoardMultipliers();
int scoreEquation(const std::vector<Json::Value> &equation,
                  const std::set<std::pair<int, int>> &newlyPlaced);