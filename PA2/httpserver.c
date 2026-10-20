/**
* @File httpserver.c
 *
 * This file contains the main function for the HTTP server.
 *
 * @author Elder Ivan Argueta
 */

#include <ctype.h>

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
    int valid_buffer = 0;
    int valid_method = 0;
    int uri_start = 0;
    int found_header = 0;
    char buffer[BUFFER_SIZE];
    int read_res = 0;

    while (continue_reading) {

        if (request.bytes_read >= BUFFER_SIZE) {
            continue_reading = 0;
            send_response(connfd, 400);
            continue;
        }

        // connfd is where to read from, buffer + bytes_read is where to place bytes at,
        read_res = read(connfd, buffer + request.bytes_read, sizeof(buffer) - request.bytes_read); // read only remaining bytes to buffer + bytes_read from connfd

        if (read_res == -1) {
            printf("Bad read in loop\n");
            send_response(connfd, 500);
            break;
        }

        if (read_res == 0) {
            continue_reading = 0;
            send_response(connfd, 400);
            continue;
        }

        request.bytes_read += read_res; // add to the amount of bytes read to the buffer

        if (request.bytes_read >= 4) {
            for (int i = 0; i <= request.bytes_read - 4; i++) {
                if (buffer[i] == '\r' && buffer[i+1] == '\n' && buffer[i+2] == '\r' && buffer[i+3] == '\n') {
                    request.header_end = i + 4;
                    if (request.header_end > 2048) {
                        valid_buffer = 0;
                        continue_reading = 0;
                        send_response(connfd, 400);
                        break;
                    }
                    valid_buffer = 1;
                    continue_reading = 0;
                    found_header = 1;
                    break;
                }
            }
        }

        if (found_header == 0  && request.bytes_read >= 2048) {
            valid_buffer = 0;
            continue_reading = 0;
            send_response(connfd, 400);
            break;
        }
    }

    if (valid_buffer) {
        for (int i = 0; i < request.header_end; i++) {
            if (i == 0) {
                if (buffer[i] == ' ') {
                    send_response(connfd, 400);
                    valid_method = 0;
                    break;
                }
            }

            if (buffer[i] == ' ') {
                request.method[i] = '\0';
                valid_method = 1;
                uri_start = i + 1;
                break;
            }

            if (i == 8) {
                send_response(connfd, 400);
                valid_method = 0;
                break;
            }

            if ((buffer[i] >= 'A' && buffer[i] <= 'Z') || (buffer[i] >= 'a' && buffer[i] <= 'z')) {
                request.method[i] = buffer[i];
            } else {
                send_response(connfd, 400);
                close(connfd);
                return;
            }
        }
    }

    if (valid_method) {
        if (strcmp(request.method, "GET") == 0) {
            // parse URI here for GET
        } else if (strcmp(request.method, "PUT") == 0) {
            // parse URI for PUT
        } else {
            send_response(connfd, 501);
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
