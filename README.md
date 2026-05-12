# Bingo Game

Multiplayer Bingo game with a C TCP server and a Python desktop client.

The server owns rooms, tickets, game state, ping/pong checks, reconnect flow, and number calling. The client connects to the server, opens a small web UI through Eel, and sends game commands over a custom binary protocol.

## Stack

- C17 server
- Python client
- Eel-based HTML/CSS/JS UI
- TCP sockets
- CMake

## Project Structure

```text
.
├── server/
│   ├── main.c
│   ├── config.cfg
│   ├── CMakeLists.txt
│   ├── includes/
│   └── src/
└── client/
    ├── main.py
    ├── modules/
    └── web/
```

## Features

- multiplayer rooms
- generated Bingo tickets
- server-side game loop
- number calling
- mark-number flow
- bingo check
- ping/pong connection tracking
- reconnect by nickname
- browser-based client UI

## Run Server

The server reads `server/config.cfg`:

```text
ip=127.0.0.1
port=8080
```

Build and run:

```sh
cd server
cmake -S . -B build
cmake --build build
./build/bingo_game
```

Run it from the `server` directory so `config.cfg` is found.

## Run Client

Install Python dependency:

```sh
cd client
python3 -m venv .venv
source .venv/bin/activate
pip install eel
```

Start a client:

```sh
python3 main.py --ip 127.0.0.1 --port 8080
```

Open multiple clients to play a room game.

## Game Flow

1. Start the C server.
2. Start several Python clients.
3. Register nicknames in the client UI.
4. Find a game.
5. Request a ticket.
6. Wait until the room is ready.
7. Mark called numbers.
8. Declare Bingo when the ticket is complete.

## Protocol

Client and server communicate through a small binary protocol.

Each message starts with a fixed header:

```text
prefix  command  length
SP      uint32   uint32
```

The payload depends on the command.

Main commands include:

- register
- find game
- ask ticket
- mark number
- bingo
- ping/pong
- room info
- reconnect
- exit room

## Notes

This is a learning/project implementation of a real-time multiplayer game. It focuses on socket programming, state handling, reconnect logic, and splitting a game into server/client responsibilities.
