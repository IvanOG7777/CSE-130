/**
* @File httpserver.c
 *
 * This file contains the main function for the HTTP server.
 *
 * @author [Put your name here]
 */

#include "listener_socket.h"
#include "iowrapper.h"
#include "protocol.h"

#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

/** @brief Handles a connection from a client.
 *
 *  @param connfd The file descriptor for the connection.
 *
 *  @return void
 */
void handle_connection(int connfd) {
    char buffer[BUFFER_SIZE];
    char method[9];
    char uri[65];
    char version[9];

    ssize_t read_res = read(connfd, buffer, sizeof(buffer));

    if (read_res < 1) {
        close(connfd);
        return;
    }

    int buffer_index = 0;
    int method_index = 0;
    int uri_index = 0;
    int version_index = 0;

    int continue_reading_method = 1;
    while (continue_reading_method) {
        while (buffer_index < read_res) {
            if (buffer[buffer_index] == '/') {
                continue_reading_method = 0;
                buffer_index++;
                break;
            }

            if (method_index >= 9) {
                fprintf(stderr, "To many for method\n");
                close(connfd);
                return;
            }

            method[method_index] = buffer[buffer_index];
            method_index++;
            buffer_index++;
        }

        if (continue_reading_method == 0) {
            break;
        }
    }

    method[method_index] = '\n';

    close(connfd);
    return;
}

/** @brief Main function for the HTTP server.
 *
 *  @param argc The number of arguments.
 *  @param argv The arguments.
 *
 *  @return EXIT_SUCCESS if successful, EXIT_FAILURE otherwise.
 */
int main(int argc, char **argv) {
    if (argc != 2) {
        warnx("wrong arguments: %s port_num", argv[0]);
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argv[1][0] == '-') {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }

    char *endptr = NULL;
    errno = 0;
                                // Pointer to first passed value. Pointer to where last read ends, base to turn number into
    size_t port = (size_t) strtoull(argv[1], &endptr, 10);

    // check port overflow
    if (errno == ERANGE) {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }
    // check if left over characters in port
    if (endptr == argv[1]) {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }

    if (*endptr != '\0') {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }

    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }

    /* Add error checking for the port number */

    signal(SIGPIPE, SIG_IGN);
    Listener_Socket_t *sock;
    sock = ls_new(port);

    // Passed port is already in use
    if (sock == NULL) {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }

    while (1) {
        int connfd = ls_accept(sock);
        if (connfd < 0) {
            continue;
        }
        handle_connection(connfd);
    }

    return EXIT_SUCCESS;
}
