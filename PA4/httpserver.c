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
#include <limits.h>

#include <sys/stat.h>

#include <pthread.h>
#include "library/queue.h"
#include "library/rwlock.h"

void handle_connection(int);

void handle_get(conn_t *);

void handle_put(conn_t *);

void handle_unsupported(conn_t *);

void *worker_thread(void *arg);

rwlock_t *get_uri_lock(const char *uri);

typedef struct uri_lock {
    char *uri; // pointer to passed uri
    rwlock_t *lock; // current rwlock
    struct uri_lock *next; // pointer to the next uri struct
} uri_lock_t;

// function that allocates a new pointer to a uri_lock struct
uri_lock_t *new_uri_lock(const char *uri, rwlock_t *lock) {
    uri_lock_t *new_lock = malloc(sizeof(uri_lock_t)); // allocate space for struct

    // if bad allocation return NULL
    if (new_lock == NULL) {
        return NULL;
    }

    // allocate memory for uri string
    new_lock->uri = malloc(strlen(uri) + 1); // allocate space for char* string

    // if bad  allocation free struct then return NULL
    if (new_lock->uri == NULL) {
        free(new_lock);
        return NULL;
    }

    // copy uri
    strcpy(new_lock->uri, uri); // copy string over to struct

    // set values and return new_lock
    new_lock->lock = lock;
    new_lock->next = NULL;

    return new_lock;
}

static uri_lock_t *lock_head = NULL; // pointer to head of linked list
static pthread_mutex_t lock_mutex =  PTHREAD_MUTEX_INITIALIZER; // used to protect the linked list when threads are searching

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

            if (val <= INT_MAX) {
                threads = (int) val;
            } else {
                fprintf(stderr, "Too big of a number\n");
                return EXIT_FAILURE;
            }
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

    // make sure port is between 1 and 65535
    if (port < 1 || port > 65535) {
        write(STDERR_FILENO, "Invalid Port\n", 13);
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

    // if queue allocation fails clean up
    if (shared_queue == NULL) {
        fprintf(stderr, "Failed to created a shared_queue\n");
        free(thread_workers);
        ls_delete(&sock);
        return EXIT_FAILURE;
    }

    // loop through threads and create each thread
    for (int i = 0; i < threads; i++) {
        int return_value = pthread_create(&thread_workers[i], NULL, worker_thread, shared_queue);
        if (return_value != 0) {
            fprintf(stderr, "Couldn't create thread\n");
        }
    }

    // single thread does all work currently
    while (1) {
        int connfd = ls_accept(sock);
        if (connfd < 0) {
            continue;
        }

        int *conn_address = (int *)malloc(sizeof(int));
        if (conn_address == NULL) {
            close(connfd);
            continue;
        }

        *conn_address = connfd;

        // push address of the current connfd to shared_queue
        if (!queue_push(shared_queue, conn_address)) {
            close(connfd);
            free(conn_address);
        }
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
        const Request_t *req = conn_get_request(conn);
        if (req == &REQUEST_PUT) {
            rwlock_t *file_lock = get_uri_lock(conn_get_uri(conn)); // get file specific lock

            if (file_lock == NULL) {
                conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR); // send error
                conn_delete(&conn); // delete conn
                return;
            }

            // If valid lock, use reader lock/unlock
            writer_lock(file_lock);
            handle_put(conn);
            writer_lock(file_lock);
        } else if (req == &REQUEST_GET) {
            rwlock_t *file_lock = get_uri_lock(conn_get_uri(conn)); // get file specific lock

            if (file_lock == NULL) {
                conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR); // send error
                conn_delete(&conn); // delete conn
                return;
            }

            // If valid lock use writer lock/unlock
            reader_lock(file_lock);
            handle_get(conn);
            reader_lock(file_lock);
        } else {
            handle_unsupported(conn);
        }
    }

    conn_delete(&conn);
}

void handle_get(conn_t *conn) {
    struct stat sb;
    char *uri = conn_get_uri(conn);

    const Response_t *res = NULL;

    int open_res = open(uri, O_RDONLY);

    if (open_res < 0) {
        if (errno == ENOENT) {
            res = &RESPONSE_NOT_FOUND;
        } else if (errno == EACCES) {
            res = &RESPONSE_FORBIDDEN;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
        }

        conn_send_response(conn, res);
        return;
    }

    // check if stat was successful
    if (fstat(open_res, &sb) < 0) {
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
        close(open_res);
        conn_send_response(conn, res);
        return;
    }

    if (!S_ISREG(sb.st_mode)) {
        res = &RESPONSE_FORBIDDEN;
        close(open_res);
        conn_send_response(conn, res);
        return;
    }

    if (sb.st_size < 0) {
        res = &RESPONSE_INTERNAL_SERVER_ERROR;
        close(open_res);
        conn_send_response(conn, res);
        return;
    }

    size_t file_size = (size_t)sb.st_size;

    res = conn_send_file(conn, open_res, file_size);

    close(open_res);
}

void handle_put(conn_t *conn) {
    char *uri = conn_get_uri(conn);

    const Response_t *res = NULL;

    // Check if file already exists before opening it.
    bool existed = access(uri, F_OK) == 0;

    // Open the file
    int fd = open(uri, O_CREAT | O_WRONLY | O_TRUNC, 0600);

    // error check on fd
    if (fd < 0) {
        // printf("%s: %d", uri, errno);
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
            res = &RESPONSE_FORBIDDEN;
            goto out; // jump to out
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            goto out; // jump to out
        }
    }

    /* fd is valid */
    // receive the file
    res = conn_recv_file(conn, fd); // conn --> fd, NULL return is good

    // check to see if it was OK or Created.
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

void handle_unsupported(conn_t *conn) {
    const Response_t *res = NULL;

    res = &RESPONSE_NOT_IMPLEMENTED;

    conn_send_response(conn, res);
}

// pass a void pointer of queue struct to function for threads to read
void *worker_thread(void *arg) {
    queue_t *shared_queue = (queue_t *) arg; // cast pointer to a queue type
    if (shared_queue == NULL) {
        return NULL;
    }

    void *item = NULL; // address of item to be popped, initially set to NULL

    // keep popping from queue
    while (1) {
        // items receives the address of dispatcher pushed connfd
        if (!queue_pop(shared_queue, &item)) {
            break;
        }
        int connfd = *(int *)item;

        free (item); // free item address from main

        handle_connection(connfd);
        close(connfd);
    }

    return NULL;
}

rwlock_t *get_uri_lock(const char *uri) {
    pthread_mutex_lock(&lock_mutex);

    uri_lock_t *current = lock_head;

    // loop through the linked list
    while (current != NULL) {
        // if a uri_struct has the passed uri
        if (strcmp(current->uri, uri) == 0) {
            // gets its lock and unlock global mutex
            rwlock_t *lock_found = current->lock;
            pthread_mutex_unlock(&lock_mutex);
            return lock_found; // return the lock
        }
        current = current->next; // keep traversing
    }

    // if no uri is found above
    rwlock_t *new_rwlock = rwlock_new(WRITERS, 0); // create new lock

    // NULL check
    if (new_rwlock == NULL) {
        pthread_mutex_unlock(&lock_mutex);
        return NULL;
    }

    // create a new
    uri_lock_t *new_lock_node= new_uri_lock(uri, new_rwlock);

    // NULL check
    if (new_lock_node == NULL) {
        rwlock_delete(&new_rwlock);
        pthread_mutex_unlock(&lock_mutex);
        return NULL;
    }

    // put new nodes next to head
    new_lock_node->next = lock_head;
    lock_head = new_lock_node; // make new node the lists head

    pthread_mutex_unlock(&lock_mutex);

    return new_rwlock;
}
