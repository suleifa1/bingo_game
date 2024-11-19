//
// Created by Ильдус on 17.11.2024.
//

#ifndef ROOM_H
#define ROOM_H

void initialize_game_rooms();

void add_client_to_room(struct Client *client, int room_id);

void assign_client_to_room(struct Client *client);

void remove_client_from_room(struct Client *client);

void replace_client_in_room(struct Client *old_client, struct Client *new_client);

void start_game_in_room(int room_id);

void end_game_in_room(struct GameRoom *room);

#endif //ROOM_H
