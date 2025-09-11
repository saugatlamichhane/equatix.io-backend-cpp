#pragma once

void reset(RoomState &room, int playerTurn);

auto checkEndGame = [](RoomState &room) {
  return (room.tileBag.empty() &&
          (room.player1Rack.empty() || room.player2Rack.empty())) ||
         room.passes == 6;
};

std::vector<std::string> createTileBag();
std::vector<std::string> drawTiles(std::vector<std::string> &tileBag, int n);