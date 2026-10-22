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
    size_t content_length;
    int has_content_length;
    int header_start;
    int header_end;
    int body_start;
    int body_end;
    int bytes_read;
    char method[9];
    char version[9];
    char uri[65];
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

    int length = snprintf(response, sizeof(response), "HTTP/1.1 %d %s\r\nContent-Length: %zu\r\n\r\n%s", code,
                          status_phrase, strlen(message_body), message_body);

    if (length < 0) return;

    if ((size_t) length >= sizeof(response)) return;

    write_n_bytes(connfd, response, length);
}

int read_header(char buffer[BUFFER_SIZE], Request *request, int connfd, int valid_buffer) {

    int read_res = 0;
    int continue_reading = 1;

    while (continue_reading) {
        if (request->bytes_read >= BUFFER_SIZE) {
            continue_reading = 0;
            send_response(connfd, 400);
            continue;
        }

        // connfd is where to read from, buffer + bytes_read is where to place bytes at,
        read_res = read(connfd, buffer + request->bytes_read, BUFFER_SIZE - request->bytes_read);
        // read only remaining bytes to buffer + bytes_read from connfd

        if (read_res == -1) {
            if (errno == EINTR) {
                continue;
            }
            send_response(connfd, 500);
            break;
        }

        if (read_res == 0) {
            continue_reading = 0;
            send_response(connfd, 400);
            continue;
        }

        request->bytes_read += read_res; // add to the amount of bytes read to the buffer

        if (request->bytes_read >= 4) {
            for (int i = 0; i <= request->bytes_read - 4; i++) {
                if (buffer[i] == '\r' && buffer[i + 1] == '\n' && buffer[i + 2] == '\r' && buffer[i + 3] == '\n') {
                    request->header_end = i + 4;

                    if (request->header_end > 2048) {
                        valid_buffer = 0;
                        continue_reading = 0;
                        send_response(connfd, 400);
                        break;
                    }

                    valid_buffer = 1;
                    continue_reading = 0;
                    break;
                }
            }
        }

        if (request->header_end == 0 && request->bytes_read >= 2048) {
            valid_buffer = 0;
            continue_reading = 0;
            send_response(connfd, 400);
            break;
        }
    }

    return valid_buffer;
}

int read_method(char buffer[BUFFER_SIZE], Request *request, int connfd, int valid_buffer, int valid_method, int *uri_start) {
    // if we have made a valid header
    int error_sent = 0;
    if (valid_buffer) {
        // loop until end of valid header
        for (int i = 0; i < request->header_end; i++) {
            if (i == 0) {
                // if first char is a space
                if (buffer[i] == ' ') {
                    // send bad request error
                    send_response(connfd, 400);
                    valid_method = 0;
                    error_sent = 1;
                    break;
                }
            }

            // check if we've seen a space indicating end of method
            if (buffer[i] == ' ') {
                request->method[i] = '\0';
                valid_method = 1;
                *uri_start = i + 1;
                break;
            }

            // if i is greater than method buffer
            // send 400 error
            if (i == 8) {
                send_response(connfd, 400);
                valid_method = 0;
                error_sent = 1;
                break;
            }

            // only accept valid chars
            if ((buffer[i] >= 'A' && buffer[i] <= 'Z') || (buffer[i] >= 'a' && buffer[i] <= 'z')) {
                request->method[i] = buffer[i];
            } else {
                // send bad request error
                send_response(connfd, 400);
                valid_method = 0;
                error_sent = 1;
                break;
            }
        }
    }

    if (!valid_method && !error_sent) {
        send_response(connfd, 400);
    }

    return valid_method;
}

int read_uri(char buffer[BUFFER_SIZE], Request *request, int connfd, int valid_method, int valid_uri, const int *uri_start, int *version_start) {
    int uri_index = 0;
    int error_sent = 0;

    // if we have made a valid method
    if (valid_method) {
        if (buffer[*uri_start] == '/') {
            // check if char at uri_start is a /
            request->uri[uri_index++] = '/';
            for (int i = *uri_start + 1; i < request->header_end; i++) {
                // check for space char to end uri
                if (buffer[i] == ' ') {
                    if (uri_index < 2) {
                        send_response(connfd, 400);
                        valid_uri = 0;
                        error_sent = 1;
                        break;
                    }
                    request->uri[uri_index] = '\0';
                    valid_uri = 1;
                    *version_start = i + 1;
                    break;
                }

                // if i reaches size of uri, and we haven't made uri
                // send 400 error
                if (uri_index >= 64) {
                    send_response(connfd, 400);
                    valid_uri = 0;
                    error_sent = 1;
                    break;
                }

                // only accept valid characters
                if ((buffer[i] >= 'A' && buffer[i] <= 'Z') ||
                    (buffer[i] >= 'a' && buffer[i] <= 'z') ||
                    buffer[i] == '.' ||
                    buffer[i] == '-' || isdigit(buffer[i])) {
                    request->uri[uri_index++] = buffer[i];
                    } else {
                        // else send bad request error
                        send_response(connfd, 400);
                        valid_uri = 0;
                        error_sent = 1;
                        break;
                    }
            }
        } else {
            send_response(connfd, 400);
            valid_uri = 0;
            error_sent = 1;
        }
    }

    if (!valid_uri && !error_sent) {
        send_response(connfd, 400);
    }

    return valid_uri;
}

int read_version(char buffer[BUFFER_SIZE], Request *request, int connfd, int valid_uri, int valid_version_length, int *version_start) {
    int version_index = 0;
    int error_sent = 0;
    // check if we have a valid uri
    if (valid_uri) {
        // loop starting from version start to end of header
        for (int i = *version_start; i < request->header_end; i++) {
            // check i + 1 is still less than header_end AND if buffer at i is \r AND if buffer at i + 1 is \n
            if (i + 1 < request->header_end && buffer[i] == '\r' && buffer[i + 1] == '\n') {

                // check if index is not correct version size
                if (version_index != 8) {
                    // send error code
                    send_response(connfd, 400);
                    error_sent = 1;
                    break;
                }
                // add null terminator to version
                request->version[version_index] = '\0';
                request->header_start = i + 2;
                valid_version_length = 1; // found valid version
                break;
            }

            // if valid index is greater than or equal to 8
            if (version_index >= 8) {
                // send 400 error code
                send_response(connfd, 400);
                error_sent = 1;
                break;
            }
            // finally add to version array
            request->version[version_index++] = buffer[i];
        }

        if (!valid_version_length && !error_sent) {
            send_response(connfd, 400);
        }
    }

    return valid_version_length;
}

int check_version(Request *request, int connfd, int valid_version_length, int valid_version) {
    if (valid_version_length) {
        // check if request.version has valid HTTP/ and digit.digit
        if (strncmp(request->version, "HTTP/", 5) != 0 || !isdigit(request->version[5]) || request->version[6] != '.' || !isdigit(request->version[7])) {
            send_response(connfd, 400);
            valid_version = 0;
        } else {
            // check if version has 1 for both digit values in version
            if (request->version[5] != '1' || request->version[7] != '1') {
                // if not send 505 error code
                send_response(connfd,505);
                valid_version = 0;
            } else {
                // else we have a valid version
                valid_version = 1;
            }
        }
    }

    return valid_version;
}

int read_content(char buffer[BUFFER_SIZE], Request *request, int connfd) {
    int valid_content = 0;
    int error_sent = 0;

    int line_start = request->header_start;
    int i = line_start;

    while (i < request->header_end) {
        if (i + 1 < request->header_end && buffer[i] == '\r' && buffer[i + 1] == '\n') {
            int line_length = i - line_start;

            if (line_length == 0) {
                valid_content = 1;
            }



            line_start = i + 2;
            i = line_start;
        } else {
            i++;
        }
    }

    if (!valid_content && !error_sent) {
        send_response(connfd, 400);
    }

    return valid_content;
}

void handle_connection(int connfd) {
    Request request;
    request.bytes_read = 0;
    request.content_length = 0;
    request.has_content_length = 0;
    request.header_start = 0;
    request.header_end = 0;

    int valid_buffer = 0;
    int valid_method = 0;
    int valid_uri = 0;
    int valid_version = 0;

    int uri_start = 0;

    int version_start = 0;
    int valid_version_length = 0;

    char buffer[BUFFER_SIZE];

    valid_buffer = read_header(buffer, &request, connfd, valid_buffer);

    if (valid_buffer) {
        valid_method = read_method(buffer, &request, connfd, valid_buffer, valid_method, &uri_start);
    } else {
        close(connfd);
        return;
    }

    if (valid_method) {
        valid_uri = read_uri(buffer, &request, connfd, valid_method, valid_uri, &uri_start, &version_start);
    } else {
        close(connfd);
        return;
    }

    if (valid_uri) {
        valid_version_length = read_version(buffer, &request, connfd, valid_uri, valid_version_length, &version_start);
    } else {
        close(connfd);
        return;
    }

    if (valid_version_length) {
        valid_version = check_version(&request, connfd, valid_version_length, valid_version);
    } else {
        close(connfd);
        return;
    }

    if (!valid_version) {
        close(connfd);
        return;
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
