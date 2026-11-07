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
    int waiting_readers; // tracks the waiting readers
    int waiting_writers; // tracks the waiting writers
    bool writer_active;

    uint32_t total_readers; // how many readers have been admitted for batch between writers
    uint32_t internal_n; // count for batched readers per writer

    PRIORITY rw_lock_type;

    pthread_mutex_t internal_mutex;

    // notification for other threads
    pthread_cond_t no_readers; // condition writers wait for
    pthread_cond_t no_writers; // condition readers wait for
};

rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {
    rwlock_t *new_rwlock = malloc(sizeof(rwlock_t)); // allocate space for struct

    if (new_rwlock == NULL) return NULL;

    // Init struct variables
    new_rwlock->total_active_readers = 0;
    new_rwlock->total_readers = 0;
    new_rwlock->waiting_readers = 0;
    new_rwlock->waiting_writers = 0;
    new_rwlock->writer_active = false;

    // Cases if rwlock is N-WAY
    if (p == N_WAY) {
        new_rwlock->internal_n = n;
    } else {
        new_rwlock->internal_n = 0;
    }
    new_rwlock->rw_lock_type = p;

    // pthread inits
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
    // check if pointer to address and struct aren't null, and free struct/address
    if (rw != NULL && *rw != NULL) {
        pthread_mutex_destroy(&(*rw)->internal_mutex);
        pthread_cond_destroy(&(*rw)->no_readers);
        pthread_cond_destroy(&(*rw)->no_writers);

        free(*rw);
        *rw = NULL;
    }
}

void reader_lock(rwlock_t *rw) {
    // lock mutex
    pthread_mutex_lock(&rw->internal_mutex);

    if (rw->rw_lock_type == READERS) {
        // wait only if there's a current writer holding the lock
        while (rw->writer_active == true) {
            // add to waiting reader list, wait for signal and subtract from list when signaled
            rw->waiting_readers++;
            pthread_cond_wait(&rw->no_writers, &rw->internal_mutex);
            rw->waiting_readers--;
        }

        rw->total_active_readers++;
    }

    if (rw->rw_lock_type == WRITERS) {
        // wait if there's a current writer holding the lock OR there are writers that are waiting
        while (rw->writer_active == true || rw->waiting_writers > 0) {
            // add to waiting reader list, wait for signal and subtract from list when signaled
            rw->waiting_readers++;
            pthread_cond_wait(&rw->no_writers, &rw->internal_mutex);
            rw->waiting_readers--;
        }

        rw->total_active_readers++;
    }

    if (rw->rw_lock_type == N_WAY) {
        // wait if there is an active writer OR if there are waiting writers AND the batch of readers has been exhausted
        while (rw->writer_active == true || (rw->waiting_writers > 0 && rw->total_readers >= rw->internal_n)) {
            rw->waiting_readers++;
            pthread_cond_wait(&rw->no_writers, &rw->internal_mutex);
            rw->waiting_readers--;
        }
        rw->total_readers++;
        rw->total_active_readers++;
    }

    pthread_mutex_unlock(&rw->internal_mutex);
}

void reader_unlock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->internal_mutex);

    if (rw->rw_lock_type == N_WAY) {
        rw->total_active_readers--;
        // broadcast to reader threads if
        // no active writer AND there are waiting reader threads && (batch hasn't been exhausted OR there are no waiting writers
        if (rw->writer_active == false && rw->waiting_readers > 0 && (rw->total_readers < rw->internal_n || rw->waiting_writers == 0)) {
            pthread_cond_broadcast(&rw->no_writers);
        }

        // signal to a writer thread if
        // active readers is 0 AND there are waiting writers && (reader batch has been exhausted OR there are no waiting readers)
        if (rw->total_active_readers == 0 && rw->waiting_writers > 0 && (rw->total_readers >= rw->internal_n || rw->waiting_readers == 0)) {
            pthread_cond_signal(&rw->no_readers);
        }
    } else {
        // case for both READER and WRITERS

        rw->total_active_readers--;

        // when last reader "unlocks"
        if (rw->total_active_readers == 0) {
            pthread_cond_signal(&rw->no_readers); // signal a waiting writer if any
        }
    }

    pthread_mutex_unlock(&rw->internal_mutex);
}

void writer_lock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->internal_mutex);

    if (rw->rw_lock_type == READERS) {
        // make writer wait if
        // there is already a writer OR there are current readers OR there are waiting readers
        while (rw->writer_active == true || rw->total_active_readers > 0 || rw->waiting_readers > 0) {
            rw->waiting_writers++;
            pthread_cond_wait(&rw->no_readers, &rw->internal_mutex);
            rw->waiting_writers--;
        }

        rw->writer_active = true;
    }

    if (rw->rw_lock_type == WRITERS) {
        while (rw->writer_active == true || rw->total_active_readers > 0) {
            rw->waiting_writers++;
            pthread_cond_wait(&rw->no_readers, &rw->internal_mutex);
            rw->waiting_writers--;
        }

        rw->writer_active = true;
    }

    if (rw->rw_lock_type == N_WAY) {

    }

    pthread_mutex_unlock(&rw->internal_mutex);
}

void writer_unlock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->internal_mutex);



    pthread_mutex_unlock(&rw->internal_mutex);
}
