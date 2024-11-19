#include "../includes/common.h"
#include "../includes/client.h"

#include <stdio.h>
#include <string.h>


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
}