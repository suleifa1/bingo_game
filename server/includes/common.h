#ifndef COMMON_H
#define COMMON_H

#include <arpa/inet.h>

#define MAX_CLIENTS 12
#define TICKET_SIZE 5
#define PING_INTERVAL 5
#define TIMEOUT 15
#define RECONNECT_TIMEOUT 100
#define MAX_NICKNAME_LEN 20
#define MAX_ROOMS (MAX_CLIENTS / 3)
#define MAX_PLAYERS_IN_ROOM (MAX_CLIENTS / MAX_ROOMS)
#define MAX_NUMBERS 40

#define DEFAULT_IP "0.0.0.0"
#define DEFAULT_PORT 4242



enum ClientState {
    STATE_REGISTER,
    STATE_LOBBY,
    STATE_RECONNECTING,
    STATE_DISCONNECTED,

    STATE_WAITING_FOR_GAME,

    STATE_GOT_TICKET,
    STATE_GET_NUMBER,
    STATE_MARK_NUMBER,

    STATE_BINGO_CHECK,
    STATE_WIN_GAME,
    STATE_LOSE_GAME
};

enum GameState {
    GAME_WAITING,
    GAME_PLAY,
    GAME_FINISH,

    GAME_SEND_NUMBER,
    GAME_WAIT_FOR_MARK,
    GAME_CHECK_BINGO
};

enum CommandType {
    CMD_REGISTER,
    CMD_READY, //?
    CMD_MARK_NUMBER,
    CMD_BINGO,
    CMD_RECONNECT,
    CMD_PONG,
    CMD_ASK_TICKET,
    TOTAL_CMD,

    CMDS_SEND_TICKET,
    CMDS_START_GAME,
    CMDS_SEND_NUMBER,
    CMDS_BINGO_CHECK,
    CMDS_PING,
    CMDS_OK,
    CMDS_END_GAME,
    CMDS_USER_INFO,
    CMD_FINDGAME,
    CMDS_ROOM_INFO,
    CMD_EXIT_ROOM,
    CMD_CALL_NUM,

    TOTAL_CMDS
};

struct Client {
    char nickname[MAX_NICKNAME_LEN];
    int socket;
    int state;
    int prev_state;
    int ticket[TICKET_SIZE];
    int marked[TICKET_SIZE];
    int ticket_count;
    int room_id;
    uint32_t last_ping;
    uint32_t last_pong;
    uint32_t disconnect_time;
}__attribute__((packed));;

struct GameRoom {
    int roomId;
    enum GameState gameState;
    int playerCount;
    struct Client *players[MAX_CLIENTS / MAX_ROOMS];
    int calledNumbers[MAX_NUMBERS];
    int totalNumbersCalled;
    int availableNumbers[MAX_NUMBERS];
    int totalAvailableNumbers;
    int unpause;
};

struct RoomInfo {
    int roomId;
    int gameState;
    int playerCount;
    char playerNicknames[MAX_PLAYERS_IN_ROOM][MAX_NICKNAME_LEN];
    int calledNumbers[MAX_NUMBERS];
    int totalNumbersCalled;
    int unpause;
} __attribute__((packed));

struct MessageHeader {
    char prefix[2];
    uint32_t command;
    uint32_t length;
}__attribute__((packed));


struct Client clients[MAX_CLIENTS];
struct GameRoom gameRooms[MAX_ROOMS];


#endif //COMMON_H
