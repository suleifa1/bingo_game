//
// Created by Ильдус on 17.11.2024.
//

#ifndef SERVER_H
#define SERVER_H

#include <stdint.h>
#include "common.h"
#include <netinet/in.h>

int initialize_server(struct sockaddr_in *address);

void send_called_numbers(struct Client *client);

void send_room_info(struct Client *client);

void send_number(struct Client *client, int number);

void initialize_game_rooms();

void generate_available_numbers(struct GameRoom *room);

void manage_game_play(struct GameRoom *room);

void send_end_game(struct Client *client);

void send_user_info(struct Client *client);

void end_game_in_room(struct GameRoom *room);

void manage_game_rooms();

void generate_ticket(struct Client *client);

void send_ticket(struct Client *client);

void send_ok(int socket);

void send_ping(int client_index);

void add_new_client(int new_socket, struct sockaddr_in *address);

void handle_ping_pong(int client_index);

void handle_reconnect_timeout(int client_index);

void handle_reconnect_request(int new_socket, const char* nickname, struct Client *client);

void process_command(struct Client *client, enum CommandType command, char *payload, uint32_t length);

int recv_all(struct Client *client, char *buffer, int length);

void handle_client_data(struct Client *client);




#endif //SERVER_H
