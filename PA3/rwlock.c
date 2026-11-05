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
    int waiting_writers;
    int waiting_readers;
    int n;
    bool writer_active;

    PRIORITY rw_lock_type;
    pthread_mutex_t internal_mutex;
    pthread_cond_t no_writers;
    pthread_cond_t no_readers;
};

rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {
    rwlock_t *new_rwlock = malloc(sizeof(rwlock_t));

    if (new_rwlock == NULL) return NULL;


}