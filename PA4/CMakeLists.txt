CMAKE_MINIMUM_REQUIRED(VERSION 3.28)

project(MultiThreadHTTP)

set(CMAKE_C_STANDARD 17)

add_executable(MultiThreadHTTP
        library/connection.h
        library/iowrapper.h
        library/listener_socket.h
        library/protocol.h
        library/queue.h
        library/request.h
        library/response.h
        library/rwlock.h

        httpserver.c
        asgn4_helper_funcs.a
)