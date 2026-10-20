/**
* @File httpserver.c
 *
 * This file contains the main function for the HTTP server.
 *
 * @author Elder Ivan Argueta
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

// Items that are sent in a request from client
typedef struct Request_t {
    char method[9];
    char uri[65];
    char version[9];
    size_t content_length;
    int has_content_length;
    int header_end;
    int bytes_read;
} Request;

void send_response(int connfd, int code) {
    const char *status_phrase;
    const char *message_body;
    switch (code) {
    case 200:
        message_body = "OK\n";
        status_phrase = "OK";
        break;
    case 201:
        message_body = "Created\n";
        status_phrase = "Created";
        break;
    case 400:
        message_body = "Bad Request\n";
        status_phrase = "Bad Request";
        break;
    case 403:
        message_body = "Forbidden\n";
        status_phrase = "Forbidden";
        break;
    case 404:
        message_body = "Not Found\n";
        status_phrase = "Not Found";
        break;
    case 500:
        message_body = "Internal Server Error\n";
        status_phrase = "Internal Server Error";
        break;
    case 501:
        message_body = "Not Implemented\n";
        status_phrase = "Not Implemented";
        break;
    case 505:
        message_body = "Version Not Supported\n";
        status_phrase = "Version Not Supported";
        break;
    default:
        code = 500;
        message_body = "Internal Server Error\n";
        status_phrase = "Internal Server Error";
        break;

    }

    char response[256];

    int length = snprintf(response, sizeof(response), "HTTP/1.1 %d %s\r\nContent-Length: %zu\r\n\r\n%s", code, status_phrase, strlen(message_body), message_body);

    if (length < 0) return;

    if ((size_t)length >= sizeof(response)) return;

    write_n_bytes(connfd, response, length);
}

void handle_connection(int connfd) {
    Request request;
    request.bytes_read = 0;
    request.content_length = 0;
    request.has_content_length = 0;
    request.header_end = 0;

    int continue_reading = 1;
    char buffer[BUFFER_SIZE];
    int read_res = 0;

    while (continue_reading) {

        if (request.bytes_read == BUFFER_SIZE) {
            continue_reading = 0;
            send_response(connfd, 400);
            continue;
        }

        // connfd is where to read from, buffer + bytes_read is where to place bytes at,
        read_res = read(connfd, buffer + request.bytes_read, sizeof(buffer) - request.bytes_read); // read only remaining bytes to buffer + bytes_read from connfd

        if (read_res == -1) {
            printf("Bad read in loop\n");
            break;
        }

        if (read_res == 0) {
            printf("EOF in loop\n");
            break;
        }

        request.bytes_read += read_res; // add to the amount of bytes read to the buffer

        if (request.bytes_read >= 4) {
            for (int i = 0; i <= request.bytes_read - 4; i++) {
                if (buffer[i] == '\r' && buffer[i+1] == '\n' && buffer[i+2] == '\r' && buffer[i+3] == '\n') {
                    request.header_end = i + 4;
                    continue_reading = 0;
                    break;
                }
            }
        }
    }

    close(connfd);
    return;
}

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
