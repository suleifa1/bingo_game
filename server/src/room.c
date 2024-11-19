#include "../includes/common.h"
#include "../includes/server.h"
#include "../includes/room.h"

#include <stdio.h>
#include <string.h>
#include <sys/socket.h>


void initialize_game_rooms() {
    for (int i = 0; i < MAX_ROOMS; i++) {
        gameRooms[i].roomId = i;
        gameRooms[i].gameState = GAME_WAITING;
        gameRooms[i].playerCount = 0;
        gameRooms[i].totalNumbersCalled = 0;
        gameRooms[i].totalAvailableNumbers = 0;
        gameRooms[i].unpause = -1;
        memset(gameRooms[i].calledNumbers, 0, sizeof(gameRooms[i].calledNumbers));
        memset(gameRooms[i].players, 0, sizeof(gameRooms[i].players));
        memset(gameRooms[i].availableNumbers, 0, sizeof(gameRooms[i].availableNumbers));
    }
}

void add_client_to_room(struct Client *client, int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) {
        printf("Invalid room ID: %d\n", room_id);
        return;
    }

    struct GameRoom *room = &gameRooms[room_id];
    if (room->playerCount >= (MAX_CLIENTS / MAX_ROOMS)) {
        printf("Room %d is full, cannot add client %s\n", room_id, client->nickname);
        return;
    }

    room->players[room->playerCount++] = client;
    client->room_id = room_id;
    client->state = STATE_WAITING_FOR_GAME;
    printf("Client %s added to room %d\n", client->nickname, room_id);
}

void assign_client_to_room(struct Client *client) {
    for (int i = 0; i < MAX_ROOMS; i++) {
        if (gameRooms[i].playerCount < (MAX_CLIENTS / MAX_ROOMS)) {
            add_client_to_room(client, i);
            return;
        }
    }
    printf("No available rooms for client %s\n", client->nickname);
}

void remove_client_from_room(struct Client *client) {
    if (client->room_id == -1) return;

    struct GameRoom *room = &gameRooms[client->room_id];
    for (int i = 0; i < room->playerCount; i++) {
        if (room->players[i] == client) {
            // Сдвигаем остальных игроков в массиве
            for (int j = i; j < room->playerCount - 1; j++) {
                room->players[j] = room->players[j + 1];
            }
            room->players[room->playerCount - 1] = NULL;
            room->playerCount--;
            printf("Client %s removed from room %d\n", client->nickname, client->room_id);
            break;
        }
    }
    client->room_id = -1;
}

void replace_client_in_room(struct Client *old_client, struct Client *new_client) {
    if (old_client->room_id == -1) return;

    struct GameRoom *room = &gameRooms[old_client->room_id];

    // Находим старого клиента в массиве игроков комнаты и заменяем на нового
    for (int i = 0; i < room->playerCount; i++) {
        if (room->players[i] == old_client) {
            room->players[i] = new_client;
            printf("Replaced client %s with %s in room %d\n", old_client->nickname, new_client->nickname, old_client->room_id);
            return;
        }
    }

    // Если старый клиент не найден в комнате
    printf("Old client %s not found in room %d\n", old_client->nickname, old_client->room_id);
}

void start_game_in_room(int room_id) {
    if (room_id < 0 || room_id >= MAX_ROOMS) {
        printf("Invalid room ID: %d\n", room_id);
        return;
    }

    struct GameRoom *room = &gameRooms[room_id];
    if (room->playerCount < 2) {
        printf("Not enough players to start the game in room %d\n", room_id);
        return;
    }

    // Генерация списка чисел, которые будут разыгрываться, на основе билетов игроков
    generate_available_numbers(room);

    room->gameState = GAME_PLAY;
    room->totalNumbersCalled = 0;
    memset(room->calledNumbers, 0, sizeof(room->calledNumbers));
    printf("Game started in room %d\n", room_id);

    // Отправляем всем игрокам команду на начало игры
    for (int i = 0; i < room->playerCount; i++) {
        struct Client *client = room->players[i];
        struct MessageHeader start_msg = {"SP", CMDS_START_GAME, 0};
        if (send(client->socket, &start_msg, sizeof(start_msg), 0) < 0) {
            perror("Failed to send start game message");
        } else {
            printf("Start game message sent to client %s\n", client->nickname);
        }
    }
}

void end_game_in_room(struct GameRoom *room) {
    // Обнуляем состояние игроков и переводим их в начальное состояние
    for (int i = 0; i < room->playerCount; i++) {
        struct Client *client = room->players[i];
        if(room->gameState == GAME_FINISH) {
            if(client->state == STATE_RECONNECTING && client->prev_state != STATE_WIN_GAME) {
                client->prev_state = STATE_LOSE_GAME;
            }

            if (client->state != STATE_WIN_GAME && client->state != STATE_RECONNECTING) client->state = STATE_LOSE_GAME;

        }



        memset(client->marked, 0, sizeof(client->marked));
        client->ticket_count = 0;
        send_end_game(client);
        send_user_info(client);
    }

    // Сброс состояния комнаты
    room->gameState = GAME_WAITING;
    room->totalNumbersCalled = 0;
    room->totalAvailableNumbers = 0;
    room->playerCount = 0;
    memset(room->calledNumbers, 0, sizeof(room->calledNumbers));
    memset(room->availableNumbers, 0, sizeof(room->availableNumbers));
    printf("Game finished in room %d, resetting room\n", room->roomId);
}
