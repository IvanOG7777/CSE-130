#include <assert.h>
#include <ctype.h>

#include "library/connection.h"
#include "library/listener_socket.h"
#include "library/request.h"
#include "library/response.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>

#include <pthread.h>
#include "library/queue.h"
#include "library/rwlock.h"

void handle_connection(int);

void handle_get(conn_t *);

void handle_put(conn_t *);

void handle_unsupported(conn_t *);

void *worker_thread(void *arg);

int main(int argc, char **argv) {
    // Gunna need to change this to valid erorr responses
    if (argc < 2) {
        warnx("wrong arguments: %s port_num", argv[0]);
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int threads = 4;
    int opt;

    // accept a -t value and make sure -t has a value (t:)
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        if (opt == 't') {
                char *endptr = NULL;
                errno = 0;
                long val = strtol(optarg, &endptr, 10); // cast value starting from index after optarg to a long

                // if any of these errors appears return error
                if (errno != 0 || endptr == optarg || *endptr != '\0' || val < 1) {
                    warnx("invalid thread count: %s", optarg);
                    return EXIT_FAILURE;
                }
                threads = (int) val;
                break;
            } else {
                fprintf(stderr, "Usage: %s [-t threads] <port>\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    if (optind != argc - 1) {
        fprintf(stderr, "Usage: %s [-t threads] <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *endptr = NULL;
    size_t port = (size_t) strtoull(argv[optind], &endptr, 10);

    if (endptr && *endptr != '\0') {
        warnx("invalid port number: %s", argv[1]);
        return EXIT_FAILURE;
    }

    signal(SIGPIPE, SIG_IGN);
    Listener_Socket_t *sock = ls_new(port);

    if (!sock) {
        warnx("cannot open socket");
        return EXIT_FAILURE;
    }

    // create new array of worker threads
    pthread_t *thread_workers = malloc(threads * sizeof(pthread_t));
    assert(thread_workers != NULL);

    queue_t *shared_queue = queue_new(threads * 2);

    // loop through threads and create each thread
    for (int i = 0; i < threads; i++) {
        pthread_create(thread_workers[i], NULL, worker_thread, shared_queue);
    }

    // single thread does all work currently
    while (1) {
        int connfd = ls_accept(sock);
        handle_connection(connfd);
        close(connfd);
    }
    ls_delete(&sock);


    free(thread_workers);
    return EXIT_SUCCESS;
}

void handle_connection(int connfd) {
    conn_t *conn = conn_new(connfd);

    const Response_t *res = conn_parse(conn);

    if (res != NULL) {
        conn_send_response(conn, res);
    } else {
        printf("%s", conn_str(conn));
        const Request_t *req = conn_get_request(conn);
        if (req == &REQUEST_PUT) {
            handle_put(conn);
        }
        // add cases for other types of requests here
    }

    conn_delete(&conn);
}

void handle_put(conn_t *conn) {
    char *uri = conn_get_uri(conn);

    const Response_t *res = NULL;

    // Check if file already exists before opening it.
    bool existed = access(uri, F_OK) == 0;

    // Open the file
    int fd = open(uri, O_CREAT | O_WRONLY | O_TRUNC, 0600);

    if (fd < 0) {
        // printf("%s: %d", uri, errno);
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
            res = &RESPONSE_FORBIDDEN;
            goto out;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            goto out;
        }
    }

    /* fd is valid */
    // receive the file
    res = conn_recv_file(conn, fd); // conn --> fd, NULL return is good

    // check to see if it was OK.
    if (res == NULL) {
        if (existed) {
            res = &RESPONSE_OK;
        } else {
            res = &RESPONSE_CREATED;
        }
    }

    close(fd);

out:
    conn_send_response(conn, res);
}
