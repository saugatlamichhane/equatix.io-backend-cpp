#pragma once
#include "EchoWebsock.h"
#include "ValidationHelpers.h"
#include "BoardHelpers.h"
#include <drogon/WebSocketController.h>
#include <json/json.h>

void reset(RoomState &room, int playerTurn);
bool checkEndGame(RoomState &room);
