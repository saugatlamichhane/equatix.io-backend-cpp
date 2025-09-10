#include "BoardHelpers.h"
#include <algorithm>
#include <random>
#include <stdexcept>

std::map<std::pair<int, int>, MultiplierType> boardMultipliers;
std::map<std::string, int> tileCount{
    {"0", 1}, {"1", 1}, {"2", 1}, {"3", 1}, {"4", 1}, {"5", 1},
    {"6", 1}, {"7", 1}, {"8", 1}, {"9", 1}, {"-", 2}, {"+", 2},
    {"/", 2}, {"*", 2}, {"=", 8}
};
std::map<std::string, int> tilePoints{
    {"0", 5}, {"1", 5}, {"2", 5}, {"3", 5}, {"4", 5}, {"5", 5},
    {"6", 5}, {"7", 5}, {"8", 5}, {"9", 5}, {"-", 5}, {"+", 5},
    {"/", 5}, {"*", 5}, {"=", 5}
};

void initBoardMultipliers() {
    for (int r = 1; r <= 15; ++r)
        for (int c = 1; c <= 15; ++c)
            boardMultipliers[{r, c}] = MultiplierType::NONE;

    std::pair<int, int> tripleWordCoords[] = {{1,1},{1,8},{1,15},{8,1},{8,15},{15,1},{15,8},{15,15}};
    for (auto &coord : tripleWordCoords)
        boardMultipliers[coord] = MultiplierType::TRIPLE_EQUATION;

    std::pair<int, int> doubleWordCoords[] = {{2,2},{2,14},{3,3},{3,13},{4,4},{4,12},{5,5},{5,11},{8,8},{11,5},{11,11},{12,4},{12,12},{13,3},{13,13},{14,2},{14,14}};
    for (auto &coord : doubleWordCoords)
        boardMultipliers[coord] = MultiplierType::DOUBLE_EQUATION;

    std::pair<int, int> tripleTileCoords[] = {{2,6},{2,10},{6,2},{6,6},{6,10},{6,14},{10,2},{10,6},{10,10},{10,14},{14,6},{14,10}};
    for (auto &coord : tripleTileCoords)
        boardMultipliers[coord] = MultiplierType::TRIPLE_TILE;

    std::pair<int, int> doubleTileCoords[] = {{1,4},{1,12},{3,7},{3,9},{4,1},{4,8},{4,15},{7,3},{7,7},{7,9},{7,13},{8,4},{8,12},{9,3},{9,7},{9,9},{9,13},{12,1},{12,8},{12,15},{13,7},{13,9},{15,4},{15,12}};
    for (auto &coord : doubleTileCoords)
        boardMultipliers[coord] = MultiplierType::DOUBLE_TILE;
}

int scoreEquation(const std::vector<Json::Value> &equation,
                  const std::set<std::pair<int,int>> &newlyPlaced) {
    int wordMultiplier = 1, score = 0;
    for (auto &tile : equation) {
        int r = tile["row"].asInt();
        int c = tile["col"].asInt();
        std::string val = tile["value"].asString();
        int letterScore = tilePoints.at(val);
        std::pair<int,int> pos = {r,c};
        if (newlyPlaced.count(pos)) {
            switch(boardMultipliers[pos]){
                case MultiplierType::DOUBLE_TILE: letterScore *= 2; break;
                case MultiplierType::TRIPLE_TILE: letterScore *= 3; break;
                case MultiplierType::DOUBLE_EQUATION: wordMultiplier *= 2; break;
                case MultiplierType::TRIPLE_EQUATION: wordMultiplier *= 3; break;
                default: break;
            }
        }
        score += letterScore;
    }
    return score * wordMultiplier;
}

std::vector<std::string> createTileBag() {
    std::vector<std::string> bag;
    for (auto &[symbol, count] : tileCount)
        for (int i = 0; i < count; i++)
            bag.push_back(symbol);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(bag.begin(), bag.end(), g);
    return bag;
}

std::vector<std::string> drawTiles(std::vector<std::string> &tileBag, int n) {
    if(n > (int)tileBag.size()) n = tileBag.size();
    std::vector<std::string> drawn(tileBag.end()-n, tileBag.end());
    tileBag.erase(tileBag.end()-n, tileBag.end());
    return drawn;
}
