#pragma once
#include <string>
#include <memory>
#include "RoomState.h"

class BotPlayer {
private:
    BotPlayer();
    BotPlayer(const BotPlayer&) = delete;
    BotPlayer& operator=(const BotPlayer&) = delete;
public:
    static std::unique_ptr<BotPlayer>& getInstance();
    std::string makeMove(const RoomState& state);
};