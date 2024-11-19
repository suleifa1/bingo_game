//
// Created by Ильдус on 17.11.2024.
//

#ifndef UTILS_H
#define UTILS_H
#include <netinet/in.h>


extern int temp_fd1, temp_fd2, temp_fd3;

extern char SERVER_IP[INET_ADDRSTRLEN];
extern int PORT;


int buffer_to_int(const void *buffer);

int is_invalid_ip(const char *ip);

void print_server_ip();

void reserve_descriptors();

void initialize_settings_from_config(const char *filename);

#endif //UTILS_H
