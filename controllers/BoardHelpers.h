#pragma once
#include <map>
#include <string>
#include <vector>
#include <set>
#include <json/json.h>

enum class MultiplierType {
    NONE,
    DOUBLE_TILE,
    TRIPLE_TILE,
    DOUBLE_EQUATION,
    TRIPLE_EQUATION
};

extern std::map<std::pair<int, int>, MultiplierType> boardMultipliers;
void initBoardMultipliers();

extern std::map<std::string, int> tileCount;
extern std::map<std::string, int> tilePoints;

int scoreEquation(const std::vector<Json::Value> &equation,
                  const std::set<std::pair<int, int>> &newlyPlaced);
std::vector<std::string> createTileBag();
std::vector<std::string> drawTiles(std::vector<std::string> &tileBag, int n);
