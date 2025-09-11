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

extern std::map<std::pair<int, int>, MultiplierType> boardMultipliers;

extern std::map<std::string, int> tileCount;

extern std::map<std::string, int> tilePoints;
                                    
void initBoardMultipliers();
int scoreEquation(const std::vector<Json::Value> &equation,
                  const std::set<std::pair<int, int>> &newlyPlaced);
