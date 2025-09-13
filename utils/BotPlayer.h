#pragma once
#include <string>
#include <memory>

class RoomState;
class BotPlayer {
public:
    std::string makeMove(const RoomState& state);
    BotPlayer();
};
