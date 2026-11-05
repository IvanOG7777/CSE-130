//
// Created by elder on 11/3/2026.
//

#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include "rwlock.h"

#include <stdlib.h>

struct rwlock {
    int total_active_readers;
    int waiting_readers; // tracks the waiting writers
    int waiting_writers; // tracks the waiting readers
    bool writer_active;

    int total_readers; // how many readers have been admitted for batch between writers
    uint32_t internal_n; // count for batched readers per writer

    PRIORITY rw_lock_type;

    pthread_mutex_t internal_mutex;

    // notification for other threads
    pthread_cond_t no_readers;
    pthread_cond_t no_writers;
};

rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {

    rwlock_t *new_rwlock = malloc(sizeof(rwlock_t));

    if (new_rwlock == NULL) return NULL;

    new_rwlock->total_active_readers = 0;
    new_rwlock->total_readers = 0;
    new_rwlock->waiting_readers = 0;
    new_rwlock->waiting_writers = 0;
    new_rwlock->writer_active = false;
    if (p == N_WAY) {
        new_rwlock->internal_n = n;
    } else {
        new_rwlock->internal_n = 0;
    }
    new_rwlock->rw_lock_type = p;

    if (pthread_mutex_init(&new_rwlock->internal_mutex, NULL) != 0) {
        free(new_rwlock);
        new_rwlock = NULL;

        return NULL;
    }

    if (pthread_cond_init(&new_rwlock->no_readers, NULL) != 0) {
        pthread_mutex_destroy(&new_rwlock->internal_mutex);

        free(new_rwlock);
        new_rwlock = NULL;

        return NULL;
    }

    if (pthread_cond_init(&new_rwlock->no_writers, NULL) != 0) {
        pthread_mutex_destroy(&new_rwlock->internal_mutex);
        pthread_cond_destroy(&new_rwlock->no_readers);

        free(new_rwlock);
        new_rwlock = NULL;

        return NULL;
    }

    return new_rwlock;
}

void rwlock_delete(rwlock_t **rw) {
    if (rw != NULL && *rw != NULL) {
        pthread_mutex_destroy(&(*rw)->internal_mutex);
        pthread_cond_destroy(&(*rw)->no_readers);
        pthread_cond_destroy(&(*rw)->no_writers);

        free(*rw);
        *rw = NULL;
    }
}

void reader_lock(rwlock_t *rw) {

    pthread_mutex_lock(&rw->internal_mutex);

    // make reader wait until flag has been set to false
    while (rw->writer_active == true) {
        pthread_cond_wait(&rw->no_writers, &rw->internal_mutex); // release mutex and notify thread when done
    }

    rw->total_active_readers++;

    pthread_mutex_unlock(&rw->internal_mutex);
}