#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#define PORT 4242
#define MAX_CLIENTS 12
#define TICKET_SIZE 5
#define PING_INTERVAL 5
#define TIMEOUT 15
#define RECONNECT_TIMEOUT 30
#define MAX_NICKNAME_LEN 20
#define MAX_ROOMS (MAX_CLIENTS / 3)
#define MAX_NUMBERS 40

enum ClientState {
    STATE_REGISTER,
    STATE_LOBBY,
    STATE_RECONNECTING,
    STATE_DISCONNECTED,

    STATE_WAITING_FOR_GAME,

    STATE_GOT_TICKET,
    STATE_GET_NUMBER,
    STATE_MARK_NUMBER,

    STATE_BINGO_CHECK
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
    CMDS_INFO,
    CMD_FINDGAME,
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

struct MessageHeader {
    char prefix[2];
    uint32_t command;
    uint32_t length;
}__attribute__((packed));


struct Client clients[MAX_CLIENTS];
struct GameRoom gameRooms[MAX_ROOMS];

int buffer_to_int(const void *buffer) {
    return *((int *)buffer);
}

void print_client_info(const struct Client *client) {
    if(client->state == STATE_DISCONNECTED) return;
    printf("\n--- Client Info ---\n");
    printf("Nickname: %s\n", client->nickname);
    printf("Socket FD: %d\n", client->socket);
    printf("Current State: %d\n", client->state);
    printf("Previous State: %d\n", client->prev_state);
    printf("Ticket Count: %d\n", client->ticket_count);
    printf("Last Ping: %u\n", client->last_ping);
    printf("Last Pong: %u\n", client->last_pong);
    printf("Disconnect Time: %u\n", client->disconnect_time);
    // printf("Tickets: \n");
    // for (int i = 0; i < MAX_TICKET_NUMBER; i++) {
    //     printf("  Ticket %d: ", i + 1);
    //     for (int j = 0; j < TICKET_SIZE; j++) {
    //         printf("%d ", client->ticket[i][j]);
    //     }
    //     printf("\n");
    // }
    // printf("Marked Numbers: \n");
    // for (int i = 0; i < MAX_TICKET_NUMBER; i++) {
    //     printf("  Ticket %d: ", i + 1);
    //     for (int j = 0; j < TICKET_SIZE; j++) {
    //         printf("%d ", client->marked[i][j]);
    //     }
    //     printf("\n");
    // }
    // printf("-------------------\n\n");
}



/// {client functions}
int initialize_server(struct sockaddr_in *address) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address->sin_family = AF_INET;
    address->sin_addr.s_addr = INADDR_ANY;
    address->sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *) address, sizeof(*address)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 3) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server initialized and listening on %s:%d...\n", inet_ntoa(address->sin_addr), PORT);
    return server_socket;
}

void initialize_single_client(struct Client *client) {
    client->socket = 0;
    client->state = STATE_DISCONNECTED;
    client->prev_state = STATE_DISCONNECTED;
    client->ticket_count = 0;
    client->last_ping = 0;
    client->last_pong = 0;
    client->disconnect_time = 0;
    client->room_id = -1;


    strncpy(client->nickname, "idle", MAX_NICKNAME_LEN);
    client->nickname[MAX_NICKNAME_LEN - 1] = '\0';


    memset(client->ticket, 0, sizeof(client->ticket));
    memset(client->marked, 0, sizeof(client->marked));
}

void initialize_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        initialize_single_client(&clients[i]);
    }
}
/// {client functions}

void send_number(struct Client *client, int number) {
    struct MessageHeader header = {"SP", CMDS_SEND_NUMBER, sizeof(number)};
    if (send(client->socket, &header, sizeof(header), 0) < 0) {
        perror("Failed to send number header");
        return;
    }

    if (send(client->socket, &number, sizeof(number), 0) < 0) {
        perror("Failed to send number");
    } else {
        printf("Number %d sent to client %s\n", number, client->nickname);
    }

    // Переводим клиента в состояние STATE_MARK_NUMBER
    client->state = STATE_MARK_NUMBER;
}


/// {room functions}
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

void generate_available_numbers(struct GameRoom *room) {
    room->totalAvailableNumbers = 0;
    for (int i = 0; i < room->playerCount; i++) {
        struct Client *client = room->players[i];
        for (int k = 0; k < TICKET_SIZE; k++) {
            int number = client->ticket[k];
            int is_unique = 1;
            for (int l = 0; l < room->totalAvailableNumbers; l++) {
                if (room->availableNumbers[l] == number) {
                    is_unique = 0;
                    break;
                }
            }
            if (is_unique) {
                room->availableNumbers[room->totalAvailableNumbers++] = number;
            }
        }
    }
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

void manage_game_play(struct GameRoom *room) {
    time_t current_time = time(NULL);
    int bingo_found;
    switch (room->gameState) {
        case GAME_SEND_NUMBER:
            if (room->totalAvailableNumbers > 0) {
                // Вызов нового номера
                int number = room->availableNumbers[room->totalAvailableNumbers - 1];
                room->calledNumbers[room->totalNumbersCalled++] = number;
                room->totalAvailableNumbers--;

                // Отправка номера всем игрокам в комнате и перевод их в STATE_MARK_NUMBER
                for (int i = 0; i < room->playerCount; i++) {
                    struct Client *client = room->players[i];
                    send_number(client,number);
                }

                // Переход к состоянию ожидания отметки
                room->gameState = GAME_WAIT_FOR_MARK;
                room->unpause = current_time + 5; // Устанавливаем паузу на 20 секунд
            } else {
                // Если нет доступных номеров, проверяем, можно ли завершить игру
                room->gameState = GAME_FINISH;
            }
            break;

        case GAME_WAIT_FOR_MARK:
            if (room->unpause != -1 && current_time >= room->unpause) {
                room->unpause = -1;
            }

            // Проверяем, истекло ли время ожидания отметок от всех клиентов
            int all_marked = 1;
            for (int i = 0; i < room->playerCount; i++) {
                struct Client *client = room->players[i];
                if (client->state == STATE_MARK_NUMBER) {
                    if (room->unpause == -1) {
                        // Сервер отмечает номер за клиента, если истек таймаут
                        for (int j = 0; j < TICKET_SIZE; j++) {
                            if (client->ticket[j] == room->calledNumbers[room->totalNumbersCalled - 1] && client->marked[j] == 0) {
                                client->marked[j] = 1;
                                break;
                            }
                        }
                        client->state = STATE_BINGO_CHECK;
                    } else {
                        all_marked = 0; // Если хотя бы один клиент еще не отметил и не истек таймаут
                    }
                }
            }

            // Если все клиенты либо отметили номер, либо сервер отметил за них
            if (all_marked) {
                room->gameState = GAME_CHECK_BINGO;
            }
            break;

        case GAME_CHECK_BINGO:
            // Проверяем, объявил ли кто-то BINGO
            bingo_found = 0;
            for (int i = 0; i < room->playerCount; i++) {
                struct Client *client = room->players[i];
                if (client->state == STATE_BINGO_CHECK) {
                    int bingo = 1;
                    for (int j = 0; j < TICKET_SIZE; j++) {
                        if (client->marked[j] == 0) {
                            bingo = 0;
                            break;
                        }
                    }
                    if (bingo) {
                        printf("Client %s declares BINGO and wins!\n", client->nickname);
                        room->gameState = GAME_FINISH;
                        bingo_found = 1;
                        break;
                    }
                }
            }

            // Если никто не объявил BINGO, сервер проверяет билеты всех клиентов
            if (!bingo_found) {
                for (int i = 0; i < room->playerCount; i++) {
                    struct Client *client = room->players[i];
                    int server_bingo_check = 1;
                    for (int j = 0; j < TICKET_SIZE; j++) {
                        if (client->marked[j] == 0) {
                            server_bingo_check = 0;
                            break;
                        }
                    }
                    if (server_bingo_check) {
                        printf("Server confirms BINGO for client %s!\n", client->nickname);
                        room->gameState = GAME_FINISH;
                        bingo_found = 1;
                        break;
                    }
                }
            }

            // Если ни у кого нет BINGO, начинаем новый раунд с выдачи следующего номера
            if (!bingo_found) {
                for (int i = 0; i < room->playerCount; i++) {
                    room->players[i]->state = STATE_GET_NUMBER;
                }
                room->gameState = GAME_SEND_NUMBER;
            }
            break;

        default:
            break;
    }
}

void end_game_in_room(struct GameRoom *room) {
    // Обнуляем состояние игроков и переводим их в начальное состояние
    for (int i = 0; i < room->playerCount; i++) {
        struct Client *client = room->players[i];
        client->state = STATE_LOBBY;
        memset(client->marked, 0, sizeof(client->marked));
        client->ticket_count = 0;
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

void manage_game_rooms() {
    for (int room_index = 0; room_index < MAX_ROOMS; room_index++) {
        struct GameRoom *room = &gameRooms[room_index];
        int all_ready;

        // Пропускаем комнату, если в ней нет игроков
        if (room->playerCount == 0) {
            continue;
        }

        // Управляем состоянием комнаты в зависимости от текущего состояния игры
        switch (room->gameState) {
            case GAME_WAITING:
                // Проверяем, готовы ли все игроки в комнате
                all_ready = 1;
                for (int i = 0; i < room->playerCount; i++) {
                    struct Client *client = room->players[i];
                    if (client->state != STATE_GOT_TICKET || room->playerCount != 3) {
                        all_ready = 0;
                        break;
                    }
                }
                //printf("count of players in room: %d, ready: %s\n",room->playerCount, all_ready == 1 ? "true":"false");

                // Если все игроки готовы, начинаем игру
                if (all_ready) {
                    start_game_in_room(room_index);
                }
                break;

            case GAME_WAIT_FOR_MARK:
            case GAME_CHECK_BINGO:
            case GAME_SEND_NUMBER:
            case GAME_PLAY:
                // Управление процессом игры
                if(room->gameState == GAME_PLAY) room->gameState = GAME_SEND_NUMBER;

                printf("dDENISADASDASDASD\n");
                manage_game_play(room);
                break;

            case GAME_FINISH:
                // Завершение игры и обнуление состояния комнаты
                end_game_in_room(room);
                break;

            default:
                break;
        }
    }
}
/// {room functions}

/// {server side func}
void cmds_info(struct Client *client) {
    if (client->state == STATE_DISCONNECTED || client->socket <= 0 || client->state == STATE_RECONNECTING) {
        perror("Client is not connected or invalid socket\n");
        return;
    }

    // Создаем заголовок сообщения с командой CMDS_INFO и длиной структуры клиента
    struct MessageHeader header = {"SP", CMDS_INFO, sizeof(struct Client)};

    // Отправляем заголовок сообщения
    if (send(client->socket, &header, sizeof(header), 0) < 0) {
        perror("Failed to send CMDS_INFO header");
        return;
    }

    // Отправляем данные клиента
    if (send(client->socket, client, sizeof(struct Client), 0) < 0) {
        perror("Failed to send client info");
    } else {
        printf("Client info sent to client %s (socket %d)\n", client->nickname, client->socket);
    }
}

void generate_ticket(struct Client *client) {
    srand(time(NULL) + client->socket); // Инициализация генератора случайных чисел
    for (int i = 0; i < TICKET_SIZE; i++) {
        int unique;
        do {
            unique = 1;
            client->ticket[i] = rand() % MAX_NUMBERS + 1;
            // Проверка уникальности числа в билете
            for (int j = 0; j < i; j++) {
                if (client->ticket[i] == client->ticket[j]) {
                    unique = 0;
                    break;
                }
            }
        } while (!unique);
    }
    client->ticket_count = 1;
    printf("Ticket generated for client %s: ", client->nickname);
    for (int i = 0; i < TICKET_SIZE; i++) {
        printf("%d ", client->ticket[i]);
    }
    printf("\n");
}

void send_ticket(struct Client *client) {
    struct MessageHeader header = {"SP", CMDS_SEND_TICKET, sizeof(client->ticket)};
    if (send(client->socket, &header, sizeof(header), 0) < 0) {
        perror("Failed to send ticket header");
        return;
    }

    if (send(client->socket, client->ticket, sizeof(client->ticket), 0) < 0) {
        perror("Failed to send ticket");
    } else {
        printf("Ticket sent to client %s\n", client->nickname);
    }
}

void send_ok(int socket) {
    struct MessageHeader ok = {"SP", CMDS_OK, 0};

    if (send(socket, &ok, sizeof(ok), 0) < 0) {
        perror("Failed to send OK");
    } else {
        printf("OK sent to client %d\n", socket);
    }
}

void send_ping(int client_index) {
    struct MessageHeader ping = {"SP", CMDS_PING, 0};

    if (send(clients[client_index].socket, &ping, sizeof(ping), 0) < 0) {
        perror("Failed to send PING");
    } else {
        clients[client_index].last_ping = time(NULL);
        //printf("PING %d\n", clients[client_index].socket);
    }
}

void add_new_client(int new_socket, struct sockaddr_in *address) {
    int added = 0;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].state == STATE_DISCONNECTED) {
            initialize_single_client(&clients[i]);

            clients[i].socket = new_socket;
            clients[i].prev_state = STATE_REGISTER;
            clients[i].state = STATE_REGISTER;
            clients[i].last_ping = time(NULL);
            clients[i].last_pong = time(NULL);
            clients[i].disconnect_time = 0;

            printf("Added new client with socket %d at index %d (IP %s, PORT %d)\n",
                   new_socket, i, inet_ntoa(address->sin_addr), ntohs(address->sin_port));
            added = 1;
            break;
        }
    }

    if (!added) {
        printf("Server full: unable to add new client with socket %d\n", new_socket);
        close(new_socket);
    }
}

// void handle_ready_command(struct Client *client) {
//         client->state = STATE_READY;
//         printf("Client %s is ready\n", client->nickname);
//
//         struct GameRoom *room = &gameRooms[client->room_id];
//         int all_ready = 1;
//         for (int i = 0; i < room->playerCount; i++) {
//             if (room->players[i]->state != STATE_READY) {
//                 all_ready = 0;
//                 break;
//             }
//         }
//
//         if (all_ready) {
//             start_game_in_room(client->room_id);
//         }
// }

void handle_ping_pong(int client_index) {
    if (clients[client_index].state == STATE_DISCONNECTED || clients[client_index].socket == 0) return;

    time_t now = time(NULL);

    if (now - clients[client_index].last_ping >= PING_INTERVAL) {
        send_ping(client_index);
    }

    // Проверяем, истекло ли время ожидания ответа PONG

    if (now - clients[client_index].last_pong >= TIMEOUT) {
        printf("Client %d timed out, marking for reconnection\n", clients[client_index].socket);

        // Закрываем старый сокет и переводим в режим реконнекта
        close(clients[client_index].socket);
        clients[client_index].socket = 0;
        clients[client_index].prev_state = clients[client_index].state;
        clients[client_index].state = STATE_RECONNECTING;
        clients[client_index].disconnect_time = now;
    }
}

void handle_reconnect_timeout(int client_index) {
    if (clients[client_index].state != STATE_RECONNECTING) return;

    time_t now = time(NULL);

    // Проверяем, истекло ли время на реконнект
    if (now - clients[client_index].disconnect_time >= RECONNECT_TIMEOUT && clients[client_index].disconnect_time != 0) {
        printf("Client %d failed to reconnect in time, marking as disconnected\n", clients[client_index].socket);
        close(clients[client_index].socket);
        remove_client_from_room(&clients[client_index]);
        initialize_single_client(&clients[client_index]);
    }
}

void handle_reconnect_request(int new_socket, const char* nickname, struct Client *client) {
    struct Client* old_client = NULL;


    // Ищем клиента в RECONNECTING по нику
    for (int i = 0; i < MAX_CLIENTS; i++) {
        print_client_info(&clients[i]);
        if ((strcmp(clients[i].nickname, nickname) == 0 && clients[i].state == STATE_RECONNECTING )) {
            old_client = &clients[i];
            break;
        }
    }

    if (old_client) {


        // Копируем данные старого клиента в новый, сохраняем новый сокет и время
        memcpy(client, old_client, sizeof(struct Client));
        client->socket = new_socket;
        client->state = old_client->prev_state;
        client->last_ping = time(NULL);
        client->last_pong = time(NULL);
        client->disconnect_time = 0;

        remove_client_from_room(old_client);

        // Если предыдущие состояния были PLAY или GOT_NUMBER, синхронизируем состояние с другими игроками в комнате
        if (client->prev_state == STATE_GET_NUMBER || client->prev_state == STATE_GOT_TICKET) {
            struct GameRoom *room = &gameRooms[client->room_id];
            if (room) {
                // Синхронизация состояния клиента с другими игроками в комнате
                for (int i = 0; i < room->playerCount; i++) {
                    struct Client *other_client = room->players[i];
                    if (other_client != client) {
                        client->state = other_client->state;
                        client->prev_state = other_client->prev_state;
                        break;
                    }
                }

                // Синхронизация состояния отмеченных чисел через информацию о вызванных номерах
                memset(client->marked, 0, sizeof(client->marked));
                for (int i = 0; i < room->totalNumbersCalled; i++) {
                    int called_number = room->calledNumbers[i];
                    for (int j = 0; j < TICKET_SIZE; j++) {
                        if (client->ticket[j] == called_number) {
                            client->marked[j] = 1;
                        }
                    }
                }
            }
        }

        printf("Client %s reconnected with new socket FD %d\n", client->nickname, new_socket);

        // Обнуляем старый клиент
        initialize_single_client(old_client);


    } else {
        printf("Reconnect failed: client %s not found in RECONNECTING state\n", nickname);
        //close(new_socket);  // Закрываем неподключённый сокет
    }
}

void process_command(struct Client *client, enum CommandType command, char *payload, uint32_t length) {
    printf("%d\n",command);
    switch (command) {
        case CMD_REGISTER:
            if (client->state == STATE_REGISTER) {

                strncpy(client->nickname, payload, MAX_NICKNAME_LEN - 1);
                client->nickname[MAX_NICKNAME_LEN - 1] = '\0';

                handle_reconnect_request(client->socket, client->nickname, client);
                if (client->state == STATE_REGISTER) {
                    client->state = STATE_LOBBY;
                    printf("Client registered %s\n", client->nickname);
                }else {
                    print_client_info(client);
                    cmds_info(client);
                }


                // ?? send_ok(client->socket);


            } else {
                printf("Client in invalid state for registration\n");
            }
            break;

        case CMD_FINDGAME:
            if (client->state == STATE_LOBBY) {
                assign_client_to_room(client);
                // ?? send_ok(client->socket);

                printf("Client asigned to room  %s\n", client->nickname);
            } else {
                printf("Client in invalid state for find a game\n");
            }
            break;

        case CMD_ASK_TICKET:
            if (client->state == STATE_WAITING_FOR_GAME) {
                generate_ticket(client);
                send_ticket(client);
                client->state = STATE_GOT_TICKET;
            } else {
                printf("Client in invalid state for getting ticket\n");
            }
            break;

        case CMD_MARK_NUMBER:
            if (client->state == STATE_MARK_NUMBER) {
                int number = buffer_to_int(payload);
                printf("Client %s is trying to mark number %d\n", client->nickname, number);

                for (int i = 0; i < TICKET_SIZE; i++) {
                    if (client->ticket[i] == number && client->marked[i] == 0) {
                        client->marked[i] = 1;
                        printf("Client %s marked number %d\n", client->nickname, number);
                        break;
                    }
                }
                client->state = STATE_BINGO_CHECK;
            } else {
                printf("Client in invalid state for CMD_MARK_NUMBER\n");
            }
            break;

        case CMD_BINGO:
            if (client->state == STATE_BINGO_CHECK) {
                printf("Client says has bingo\n");
            } else {
                printf("Client in invalid state for CMD_BINGO\n");
            }
            break;

        case CMD_RECONNECT:
            if (client->state == STATE_REGISTER) {
                handle_reconnect_request(client->socket,payload,client);
                if (client->state == STATE_REGISTER){
                    process_command(client,CMD_REGISTER,payload,length);
                }else{
                    cmds_info(client);
                    printf("Reconnected\n");
                }

            } else {
                printf("Client in invalid state for CMD_RECONNECT\n");
            }
            break;

        case CMD_PONG:
            if (client->state != STATE_DISCONNECTED) {
                client->last_pong = time(NULL);
            } else {
                printf("Client in invalid state for CMD_PONG\n");
            }
            break;

        default:
            printf("Unknown command\n");
            break;
    }
}

int recv_all(struct Client *client, char *buffer, int length) {
    int total_read = 0;
    int bytes_read;

    while (total_read < length) {
        bytes_read = recv(client->socket, buffer + total_read, length - total_read, 0);

        // Проверяем, если произошел разрыв соединения
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                printf("Client %s (socket %d) disconnected\n", client->nickname, client->socket);
            } else {
                perror("recv failed");
            }

            // Если клиент был в состоянии игры, переводим его в RECONNECTING
            if ( client->state == STATE_GET_NUMBER || client->state == STATE_GOT_TICKET || client->state == STATE_MARK_NUMBER || client->state == STATE_BINGO_CHECK) {
                // Закрываем текущий сокет
                close(client->socket);
                client->socket = 0;

                // Сохраняем текущее состояние и переводим клиента в STATE_RECONNECTING
                client->prev_state = client->state;
                client->state = STATE_RECONNECTING;
                client->disconnect_time = time(NULL);
                printf("Client %s is now in STATE_RECONNECTING\n", client->nickname);
            } else {
                // Клиент в других состояниях просто переводится в STATE_DISCONNECTED
                printf("client state disc: %d\n", client->state);
                remove_client_from_room(client);
                close(client->socket);
                initialize_single_client(client);

                printf("Client %s is now in STATE_DISCONNECTED\n", client->nickname);
            }

            return -1; // Возвращаем -1, чтобы сигнализировать о разрыве соединения
        }

        total_read += bytes_read;
    }

    return total_read;
}

void handle_client_data(struct Client *client) {
    struct MessageHeader header;

    // Используем recv_all для получения заголовка сообщения
    if (recv_all(client, (char *)&header, sizeof(struct MessageHeader)) != sizeof(struct MessageHeader)) {
        // Если recv_all вернул -1, это значит, что клиент отключился, и его состояние уже обновлено внутри recv_all
        return;
    }

    // Проверка префикса
    if (strncmp(header.prefix, "SP", 2) != 0) {
        printf("Invalid prefix received from client %s\n", client->nickname);
        close(client->socket);
        client->socket = 0;
        client->state = STATE_DISCONNECTED;
        return;
    }

    char *payload = NULL;
    if (header.length > 0) {
        payload = malloc(header.length);
        if (recv_all(client, payload, header.length) != header.length) {
            free(payload);
            // Состояние клиента уже обновлено в recv_all, если произошел разрыв соединения
            return;
        }
    }

    // Обработка команды клиента
    process_command(client, header.command, payload, header.length);

    if (payload) free(payload);
}

int main(void) {
    int server_socket, new_socket, max_sd, sd, activity;
    struct sockaddr_in address;
    fd_set readfds;
    struct timeval timeout;

    server_socket = initialize_server(&address);
    initialize_clients();
    initialize_game_rooms();

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        max_sd = server_socket;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].socket;
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

        if ((activity < 0) && (errno != EINTR)) {
            printf("select error\n");
        }

        if (FD_ISSET(server_socket, &readfds)) {
            int addrlen = sizeof(address);

            do {
                if ((new_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t *) &addrlen)) < 0) {
                    perror("accept failed");
                    exit(EXIT_FAILURE);
                }
            } while (new_socket == 0);

            add_new_client(new_socket, &address);
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {

            sd = clients[i].socket;
            if (FD_ISSET(sd, &readfds)) {
                // Process client data
                handle_client_data(&clients[i]);
            }
            handle_ping_pong(i);
            handle_reconnect_timeout(i);

        }
        manage_game_rooms();

    }

    return 0;
}

