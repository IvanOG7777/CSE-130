CMAKE_MINIMUM_REQUIRED(VERSION 3.28)
project(HW2)

set(CMAKE_CXX_STANDARD 23)

add_executable(httpserver
        httpserver.c
        iowrapper.h
        listener_socket.h
        protocol.h
        asgn2_helper_funcs.a
)