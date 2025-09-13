#include "BotPlayer.h"
#include <trantor/utils/Logger.h>

BotPlayer::BotPlayer() {
    // Constructor code here (if needed)
    LOG_DEBUG << "BotPlayer instance created.";
}


std::string BotPlayer::makeMove(const RoomState& state) {
    // Simple bot logic: just return a placeholder move
    // You can implement more complex logic here
    LOG_DEBUG << "Bot is making a move.";
    return "BotMovePlaceholder";
}
