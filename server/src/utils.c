#include "../includes/utils.h"
#include "../includes/common.h"

#include <ifaddrs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/fcntl.h>

int temp_fd1, temp_fd2, temp_fd3;
char SERVER_IP[INET_ADDRSTRLEN] = DEFAULT_IP;
int PORT = DEFAULT_PORT;


int buffer_to_int(const void *buffer) {
    return *((int *)buffer);
}

int is_invalid_ip(const char *ip) {
    struct in_addr addr;
    inet_pton(AF_INET, ip, &addr);

    uint32_t ip_addr = ntohl(addr.s_addr);

    if (ip_addr >= 0x7F000000 && ip_addr <= 0x7FFFFFFF) { // loopback range
        return 1;
    }

    if ((ip_addr >= 0x0A000000 && ip_addr <= 0x0AFFFFFF) || // 10.0.0.0 - 10.255.255.255
        (ip_addr >= 0xAC100000 && ip_addr <= 0xAC1FFFFF) || // 172.16.0.0 - 172.31.255.255
        (ip_addr >= 0xC0A80000 && ip_addr <= 0xC0A8FFFF)) { // 192.168.0.0 - 192.168.255.255
        return 1;
        }

    if ((ip_addr >= 0xA9FE0000 && ip_addr <= 0xA9FEFFFF) || // 169.254.0.0 - 169.254.255.255 (APIPA)
        (ip_addr >= 0x64400000 && ip_addr <= 0x647FFFFF)) { // 100.64.0.0 - 100.127.255.255 (CGNAT)
        return 1;
        }

    return 0;
}

void print_server_ip() {
    if (strcmp(SERVER_IP, "0.0.0.0") != 0) {
        printf("Server is running on IP: %s:%d\n", SERVER_IP, PORT);
        return;
    }
    struct ifaddrs *interfaces, *iface;
    char ip[INET_ADDRSTRLEN];

    if (getifaddrs(&interfaces) == -1) {
        perror("getifaddrs failed");
        return;
    }

    iface = interfaces;
    while (iface) {
        if (iface->ifa_addr && iface->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *addr = (struct sockaddr_in *)iface->ifa_addr;
            inet_ntop(AF_INET, &addr->sin_addr, ip, sizeof(ip));

            if (!is_invalid_ip(ip)) {
                printf("Public IP: %s:%d\n", ip,PORT);
                break;
            }
        }
        iface = iface->ifa_next;
    }

    freeifaddrs(interfaces);
}

void reserve_descriptors() {
     temp_fd1 = open("/tmp/tempfile1", O_CREAT | O_RDWR, 0666);
     temp_fd2 = open("/tmp/tempfile2", O_CREAT | O_RDWR, 0666);
     temp_fd3 = open("/tmp/tempfile3", O_CREAT | O_RDWR, 0666);

    if (temp_fd1 < 0 || temp_fd2 < 0 || temp_fd3 < 0) {
        perror("Failed to open temporary files");
        exit(EXIT_FAILURE);
    }

    unlink("/tmp/tempfile1");
    unlink("/tmp/tempfile2");
    unlink("/tmp/tempfile3");
}

void initialize_settings_from_config(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Could not open config file. Using default settings.");
        return;
    }

    char line[128];
    while (fgets(line, sizeof(line), file)) {
        char key[64], value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            int option = 0;

            if (strcmp(key, "ip") == 0) option = 1;
            else if (strcmp(key, "port") == 0) option = 2;

            switch (option) {
                case 1: { // ip
                    struct in_addr addr;
                    if (inet_pton(AF_INET, value, &addr)) {
                        strncpy(SERVER_IP, value, INET_ADDRSTRLEN);
                    } else {
                        printf("Invalid IP in config: %s. Using default: %s\n", value, DEFAULT_IP);
                    }
                    break;
                }
                case 2: { // port
                    int port = atoi(value);
                    if (port > 0 && port <= 65535) {
                        PORT = port;
                    } else {
                        printf("Invalid port in config: %s. Using default: %d\n", value, DEFAULT_PORT);
                    }
                    break;
                }
                default:
                    printf("Unknown key in config: %s. Ignoring.\n", key);
                break;
            }
        }
    }

    fclose(file);
}
