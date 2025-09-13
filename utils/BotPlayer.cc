#include "BotPlayer.h"
#include <trantor/utils/Logger.h>

Botplayer::BotPlayer() {
    // Constructor code here (if needed)
    LOG_DEBUG << "BotPlayer instance created.";
}

std::unique_ptr<BotPlayer>& BotPlayer::getInstance() {
    static std::unique_ptr<BotPlayer> instance(new BotPlayer());
    return instance;
}

std::string BotPlayer::makeMove(const RoomState& state) {
    // Simple bot logic: just return a placeholder move
    // You can implement more complex logic here
    LOG_DEBUG << "Bot is making a move.";
    return "BotMovePlaceholder";
}