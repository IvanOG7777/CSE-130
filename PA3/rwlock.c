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
    int total_readers;
    int waiting_readers;
    int waiting_writers;
    uint32_t internal_n;
    bool writer_active;

    PRIORITY rw_lock_type;

    pthread_mutex_t internal_mutex;
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
    
}