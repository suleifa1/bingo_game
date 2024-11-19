cmake_minimum_required(VERSION 3.25)
project(bingo_game C)

set(CMAKE_C_STANDARD 17)

add_executable(bingo_game main2.c
        includes/common.h
        includes/server.h
        includes/client.h
        includes/utils.h
        includes/room.h
        src/server.c
        src/utils.c
        src/room.c
        src/client.c
        )
