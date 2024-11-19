#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/_select.h>
#include <sys/_types/_fd_def.h>
#include <sys/_types/_socklen_t.h>

#include "includes/client.h"
#include "includes/server.h"
#include "includes/utils.h"


int main(void) {
    initialize_settings_from_config("./config.cfg");

    signal(SIGPIPE, SIG_IGN);
    int server_socket, new_socket, max_sd, sd, activity;
    struct sockaddr_in address;
    fd_set readfds;
    struct timeval timeout;

    server_socket = initialize_server(&address);
    print_server_ip();
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

            reserve_descriptors();
            if ((new_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t *) &addrlen)) < 0) {
                perror("accept failed");
                exit(EXIT_FAILURE);
            }
            close(temp_fd1);
            close(temp_fd2);
            close(temp_fd3);
            add_new_client(new_socket, &address);
        }



        for (int i = 0; i < MAX_CLIENTS; i++) {

            sd = clients[i].socket;
            if (FD_ISSET(sd, &readfds)) {
                handle_client_data(&clients[i]);
            }
            handle_ping_pong(i);
            handle_reconnect_timeout(i);

        }
        manage_game_rooms();

    }

    return 0;
}
