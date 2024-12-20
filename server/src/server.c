#include "../includes/server.h"
#include "../includes/room.h"
#include "../includes/client.h"
#include "../includes/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int initialize_server(struct sockaddr_in *address) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address->sin_family = AF_INET;

    if (inet_pton(AF_INET, SERVER_IP, &address->sin_addr) <= 0) {
        perror("Invalid IP address in SERVER_IP");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

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
    return server_socket;
}

void send_called_numbers(struct Client *client) {
    if (client->room_id == -1) {
        printf("Client %s is not in any room\n", client->nickname);
        return;
    }

    struct GameRoom *room = &gameRooms[client->room_id];

    // Создаем заголовок сообщения с командой CMD_CALL_NUM и длиной списка вызванных чисел
    struct MessageHeader header = {"SP", CMD_CALL_NUM, sizeof(room->calledNumbers)};

    // Отправляем заголовок
    if (send(client->socket, &header, sizeof(header), 0) < 0) {
        perror("Failed to send CMD_CALL_NUM header");
        return;
    }

    // Отправляем список вызванных чисел
    if (send(client->socket, room->calledNumbers, sizeof(room->calledNumbers), 0) < 0) {
        perror("Failed to send called numbers");
    } else {
        printf("Called numbers sent to client %s\n", client->nickname);
    }
}

void send_room_info(struct Client *client) {
    if (client->room_id == -1) {
        printf("Client %s is not in any room\n", client->nickname);
        return;
    }

    struct GameRoom *room = &gameRooms[client->room_id];
    struct RoomInfo roomInfo;

    // Заполняем общую информацию о комнате
    roomInfo.roomId = room->roomId;
    roomInfo.gameState = room->gameState;
    roomInfo.totalNumbersCalled = room->totalNumbersCalled;
    roomInfo.unpause = room->unpause;

    // Инициализируем счетчик активных игроков
    int activePlayerCount = 0;

    // Копируем никнеймы только тех игроков, которые не находятся в состоянии STATE_RECONNECTING
    for (int i = 0; i < room->playerCount; i++) {
        struct Client *player = room->players[i];
        if (player->state != STATE_RECONNECTING) {
            strncpy(roomInfo.playerNicknames[activePlayerCount], player->nickname, MAX_NICKNAME_LEN);
            roomInfo.playerNicknames[activePlayerCount][MAX_NICKNAME_LEN - 1] = '\0';  // Обеспечиваем нуль-терминацию
            activePlayerCount++;
        }
    }

    // Устанавливаем количество активных игроков
    roomInfo.playerCount = activePlayerCount;

    // Копируем вызванные номера
    for (int i = 0; i < room->totalNumbersCalled; i++) {
        roomInfo.calledNumbers[i] = room->calledNumbers[i];
    }

    // Создаем заголовок сообщения с обновленным размером полезной нагрузки
    int payloadSize = sizeof(struct RoomInfo);
    struct MessageHeader header = {"SP", CMDS_ROOM_INFO, payloadSize};

    // Отправляем заголовок
    if (send(client->socket, &header, sizeof(header), 0) < 0) {
        perror("Failed to send room info header");
        return;
    }

    // Отправляем данные о комнате
    if (send(client->socket, &roomInfo, payloadSize, 0) < 0) {
        perror("Failed to send room info");
    } else {
        printf("Room info sent to client %s\n", client->nickname);
    }
}

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

    if (client->state != STATE_RECONNECTING) client->state = STATE_MARK_NUMBER;
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
                    if(client->state != STATE_RECONNECTING && client->socket != 0){
                        send_number(client,number);
                    }

                }

                // Переход к состоянию ожидания отметки
                room->gameState = GAME_WAIT_FOR_MARK;
                room->unpause = current_time + 10; // Устанавливаем паузу на 20 секунд

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
                        send_called_numbers(client);
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
                        if (client->state == STATE_RECONNECTING) {
                            client->prev_state = STATE_WIN_GAME;
                        }
                        else {
                            client->state = STATE_WIN_GAME;
                        }

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
                    if (room->players[i]->state != STATE_RECONNECTING) room->players[i]->state = STATE_GET_NUMBER;
                }
                room->gameState = GAME_SEND_NUMBER;
            }
            break;

        default:
            break;
    }
}

void send_end_game(struct Client *client) {
    struct MessageHeader end_game = {"SP", CMDS_END_GAME, 0};

    if (send(client->socket, &end_game, sizeof(end_game), 0) < 0) {
        perror("Failed to send end_game");
    } else {
        //client->socket.last_ping = time(NULL); ??
        //printf("PING %d\n", clients[client_index].socket);
    }
}

void send_user_info(struct Client *client) {
    if (client->state == STATE_DISCONNECTED || client->socket <= 0 || client->state == STATE_RECONNECTING) {
        perror("Client is not connected or invalid socket\n");
        return;
    }

    // Создаем заголовок сообщения с командой CMDS_USER_INFO и длиной структуры клиента
    struct MessageHeader header = {"SP", CMDS_USER_INFO, sizeof(struct Client)};

    // Отправляем заголовок сообщения
    if (send(client->socket, &header, sizeof(header), 0) < 0) {
        perror("Failed to send CMDS_USER_INFO header");
        return;
    }

    // Отправляем данные клиента
    if (send(client->socket, client, sizeof(struct Client), 0) < 0) {
        perror("Failed to send client info");
    } else {
        printf("Client info sent to client %s (socket %d)\n", client->nickname, client->socket);
    }
}

void manage_game_rooms() {
    for (int room_index = 0; room_index < MAX_ROOMS; room_index++) {
        struct GameRoom *room = &gameRooms[room_index];
        int all_ready;

        // Пропускаем комнату, если в ней нет игроков
        if (room->playerCount == 0) {
            if (room->gameState != GAME_WAITING) {
                end_game_in_room(room);
            }
            continue;
        }
        int count = 0;
        for (int i = 0; i < room->playerCount; i++) {
            if (room->players[i]->state == STATE_RECONNECTING) {
                    count++;
            }
        }
        if (count == MAX_PLAYERS_IN_ROOM) {
            end_game_in_room(room);
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
                }else{
                    for (int i = 0; i < room->playerCount; i++) {
                        if (room->players[i]->state != STATE_RECONNECTING) send_room_info(room->players[i]);
                    }

                }


                break;

            case GAME_WAIT_FOR_MARK:
            case GAME_CHECK_BINGO:
            case GAME_SEND_NUMBER:
            case GAME_PLAY:
                // Управление процессом игры
                if(room->gameState == GAME_PLAY) room->gameState = GAME_SEND_NUMBER;

                for (int i = 0; i < room->playerCount; i++) {
                    if (room->players[i]->state != STATE_RECONNECTING) {
                        send_room_info(room->players[i]);
                    }

                }
                manage_game_play(room);
                break;

            case GAME_FINISH:
                for (int i = 0; i < room->playerCount; i++) {
                    if (room->players[i]->state != STATE_RECONNECTING) send_room_info(room->players[i]);
                }
                // Завершение игры и обнуление состояния комнаты
                end_game_in_room(room);

                break;

            default:
                break;
        }
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
        printf("PING %d\n", clients[client_index].socket);
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
            send_user_info(&clients[i]);
            send_ping(i);
            added = 1;
            break;
        }
    }

    if (!added) {
        printf("Server full: unable to add new client with socket %d\n", new_socket);
        close(new_socket);
    }
}

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

    printf("start Search fir nikname: %s\n",client->nickname);
    // Ищем клиента в RECONNECTING по нику
    for (int i = 0; i < MAX_CLIENTS; i++) {
        print_client_info(&clients[i]);
        printf("comppre: %d\n",(strcmp(clients[i].nickname, client->nickname)));
        if ((strcmp(clients[i].nickname, client->nickname) == 0 && clients[i].state == STATE_RECONNECTING &&
        clients[i].socket != client->socket)) {
            old_client = &clients[i];
            break;
        }
    }
    printf("stop Search\n");

    if (old_client) {


        // Копируем данные старого клиента в новый, сохраняем новый сокет и время
        memcpy(client, old_client, sizeof(struct Client));
        client->socket = new_socket;
        client->state = old_client->prev_state;
        client->last_ping = time(NULL);
        client->last_pong = time(NULL);
        client->disconnect_time = 0;

        replace_client_in_room(old_client,client);


        // Если предыдущие состояния были PLAY или GOT_NUMBER, синхронизируем состояние с другими игроками в комнате
        if (client->prev_state == STATE_GET_NUMBER || client->prev_state == STATE_GOT_TICKET
        && &gameRooms[client->room_id].playerCount > 1) {

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
                send_called_numbers(client);
            }
        }


        printf("Client %s reconnected with new socket FD %d\n", client->nickname, new_socket);

        // Обнуляем старый клиент
        initialize_single_client(old_client);


    } else {
        printf("Reconnect failed: client %s not found in RECONNECTING state\n", nickname);
    }
}

void process_command(struct Client *client, enum CommandType command, char *payload, uint32_t length) {
    printf("%d\n",command);
    switch (command) {
        case CMD_REGISTER:
            if (client->state == STATE_REGISTER) {

                size_t copy_len = (length < MAX_NICKNAME_LEN - 1) ? length : MAX_NICKNAME_LEN - 1;

                strncpy(client->nickname, payload, copy_len);

                // Завершаем строку нулевым байтом
                client->nickname[copy_len] = '\0';

                handle_reconnect_request(client->socket, client->nickname, client);
                if (client->state == STATE_REGISTER) {
                    client->state = STATE_LOBBY;
                    printf("Client registered %s\n", client->nickname);
                }else {
                    print_client_info(client);
                }
                send_user_info(client);


                // ?? send_ok(client->socket);


            } else {
                printf("Client in invalid state for registration\n");
            }
            break;
        case CMD_EXIT_ROOM:
            if(client->state == STATE_GOT_TICKET || client->state == STATE_WIN_GAME || client->state == STATE_LOSE_GAME)
            {
                remove_client_from_room(client);
                client->ticket_count = 0;
                memset(client->ticket, 0, sizeof(client->ticket));
                memset(client->marked, 0, sizeof(client->marked));
                client->state = STATE_LOBBY;
                send_user_info(client);

            }
            else if (client->state == STATE_WAITING_FOR_GAME)
            {
                remove_client_from_room(client);
                client->state = STATE_LOBBY;
                send_user_info(client);
            }else{
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
                    send_user_info(client);
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

    process_command(client, header.command, payload, header.length);

    if (payload) free(payload);
}
