//
// Created by elder on 11/3/2026.
//

#include <stdio.h>
#include <pthread.h>
#include "queue.h"

#include <stdlib.h>


struct queue {
    // Shared variables between threads
    int head;
    int tail;
    //
    int buffer_size;
    void **buffer;

    pthread_mutex_t mutex; // mutex to lock a thread so others can access until done
    // thread conditionals for threads to see
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
};

queue_t *queue_new(int size) {
    if (size <= 0) return NULL;

    queue_t *new_queue = malloc(sizeof(queue_t));

    if (new_queue == NULL) return NULL;

    // Queue set up

    //Allocate enough memory for the internal bufffer
    new_queue->buffer = malloc(size * sizeof(void *));
    if (new_queue->buffer == NULL) {
        free(new_queue);
        new_queue = NULL;
        return NULL;
    }

    new_queue->buffer_size = size;
    new_queue->head = 0;
    new_queue->tail = 0;

    // init the mutex and thread conditions
    if (pthread_mutex_init(&new_queue->mutex, NULL) != 0) {
        // free queues buffer and queue before returning NULL
        free(new_queue->buffer);
        new_queue->buffer = NULL;

        free(new_queue);
        new_queue = NULL;

        return NULL;
    }
    if (pthread_cond_init(&new_queue->not_full, NULL) != 0) {
        pthread_mutex_destroy(&new_queue->mutex);

        // free queues buffer and queue before returning NULL
        free(new_queue->buffer);
        new_queue->buffer = NULL;

        free(new_queue);
        new_queue = NULL;

        return NULL;
    }
    if (pthread_cond_init(&new_queue->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&new_queue->mutex);
        pthread_cond_destroy(&new_queue->not_full);

        // free queues buffer and queue before returning NULL
        free(new_queue->buffer);
        new_queue->buffer = NULL;

        free(new_queue);
        new_queue = NULL;

        return NULL;
    }

    return new_queue;
}

void queue_delete(queue_t **q) {
    if (q != NULL && *q != NULL) {
        pthread_mutex_destroy(&(*q)->mutex);
        pthread_cond_destroy(&(*q)->not_full);
        pthread_cond_destroy(&(*q)->not_empty);

        free((*q)->buffer);
        (*q)->buffer = NULL;

        free(*q);
        *q = NULL;
    }
}

bool queue_push(queue_t *q, void *elem) {

    if (q == NULL) return false;

    // lock mutex for myself so other threads can get it
    pthread_mutex_lock(&q->mutex);

    // make producer wait while buffer is full
    while (q->tail - q->head == q->buffer_size) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    // use remainder as index to write into
    int buffer_index = q->tail % q->buffer_size;

    // add the element to array and move tail up
    q->buffer[buffer_index] = elem;
    q->tail += 1;

    // signal to consumer threads and unlock mutex
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);

    return true;
}

bool queue_pop(queue_t *q, void **elem) {

    if (q == NULL) return false;

    pthread_mutex_lock(&q->mutex);

    while (q->tail == q->head) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    int buffer_index = q->head % q->buffer_size;

    *elem = q->buffer[buffer_index];
    q->head += 1;

    // signal to producer threads and unlock mutex
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    return true;
}
