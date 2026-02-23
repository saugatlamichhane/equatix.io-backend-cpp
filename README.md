# Equatix.io: Online Multiplayer Strategy Game

Equatix.io is a high-performance, real-time strategy game backend built with C++ and the Drogon framework. It is designed to handle online multiplayer interactions, competitive matchmaking, and automated game logic.

## Core Features

* **Real-time Multiplayer**: Powered by WebSockets for low-latency communication.
* **Authentication**: Integrated Firebase Authentication with a custom filter to secure user sessions.
* **Game Engine**: Includes a board state controller, move validation helpers, and an automated AI bot for single-player or training modes.
* **Competitive Systems**: Features an Elo-based matchmaking system and global leaderboards.
* **Social & Engagement**: Supports friends lists, clan systems, and daily puzzles.

## Tech Stack

* **Backend**: C++20.
* **Framework**: Drogon HTTP Framework.
* **Build System**: CMake.
* **Database**: PostgreSQL and Redis configuration supported.
* **Deployment**: Dockerized for consistent environment setup.

## Prerequisites

* Docker and Docker Compose.
* C++20 compatible compiler (if building locally without Docker).
* CMake 3.5 or higher.

## Getting Started

### Building with Docker

The easiest way to build and run the backend server is using the provided Dockerfile:

1. **Build the Image**:
```bash
docker build -t equatix-server .

```


2. **Run the Container**:
```bash
docker run -p 5555:5555 eqautix-server

```



### Local Build

If you prefer to build directly on your machine:

1. **Configure the build**:
```bash
mkdir build && cd build
cmake ..

```


2. **Compile**:
```bash
make

```


3. **Run**:
```bash
./echo

```


*Note: Ensure `config.yaml` or `config.json` is correctly configured in your working directory before running.*

## Configuration

The server's behavior is managed via `config.yaml`. You can modify the following:

* **Listeners**: Default port is set to 5555.
* **Database**: Update the `db_clients` section with your PostgreSQL credentials.
* **Logging**: Adjust the `log_level` (DEBUG, TRACE, INFO, etc.) as needed.
