#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "rwlock.h"

struct rwlock {
    PRIORITY priority; // Priority setting for the lock (readers, writers, or equal priority)
    int n_active_readers; // Number of readers currently holding the lock
    int n_waiting_readers; // Number of readers waiting for the lock
    int n_active_writers; // Number of writers currently holding the lock
    int n_waiting_writers; // Number of writers waiting for the lock

    pthread_mutex_t lock_mutex; // Mutex to protect access to the lock state
    pthread_cond_t readers_cond; // Condition variable for reader threads
    pthread_cond_t writers_cond; // Condition variable for writer threads

    int n_way; // Additional parameter for n-way reader lock, depends on priority
    int readers_s_writer; // Counter to track the number of readers since the last writer
};

// Function to create a new rwlock_t object
rwlock_t *rwlock_new(PRIORITY p, uint32_t n) {
    // Allocate memory for the rwlock_t structure
    rwlock_t *rw = malloc(sizeof(rwlock_t));
    if (!rw) {
        return NULL;
    }

    // Initialize the rwlock_t structure
    rw->n_active_readers = 0;
    rw->n_waiting_readers = 0;
    rw->n_active_writers = 0;
    rw->n_waiting_writers = 0;
    rw->priority = p;
    rw->n_way = (p == N_WAY) ? n : 0; // Set 'n_way' based on priority
    rw->readers_s_writer = 0;

    // Initialize mutex and condition variables
    pthread_mutex_init(&rw->lock_mutex, NULL);
    pthread_cond_init(&rw->readers_cond, NULL);
    pthread_cond_init(&rw->writers_cond, NULL);

    return rw;
}

// Function to delete a rwlock_t object
void rwlock_delete(rwlock_t **rw) {
    if (rw && *rw) {
        // Destroy mutex and condition variables
        pthread_mutex_destroy(&(*rw)->lock_mutex);
        pthread_cond_destroy(&(*rw)->readers_cond);
        pthread_cond_destroy(&(*rw)->writers_cond);
        free(*rw);
        *rw = NULL;
    }
}

// Function for a reader to acquire the lock
void reader_lock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->lock_mutex);
    rw->n_waiting_readers++;

    // Enter loop if conditions are met
    for (;;) {
        if (rw->n_active_writers == 0
            && !(rw->priority == WRITERS && rw->n_waiting_writers > 0
                 && rw->n_active_readers == 0)) {
            break;
        }
        // Wait for the readers condition to be signaled
        pthread_cond_wait(&rw->readers_cond, &rw->lock_mutex);
    }

    rw->n_waiting_readers--;
    rw->n_active_readers++;
    pthread_mutex_unlock(&rw->lock_mutex);
}

// Function for a reader to release the lock
void reader_unlock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->lock_mutex);
    rw->n_active_readers--;

    // If there are no active readers and there are waiting writers, signal one writer
    if (rw->n_active_readers == 0 && rw->n_waiting_writers > 0) {
        pthread_cond_signal(&rw->writers_cond);
    }
    pthread_mutex_unlock(&rw->lock_mutex);
}

// Function for a writer to acquire the lock
void writer_lock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->lock_mutex);
    rw->n_waiting_writers++;

    // Wait while any readers or writers are active
    while (rw->n_active_readers > 0 || rw->n_active_writers > 0) {
        pthread_cond_wait(&rw->writers_cond, &rw->lock_mutex);
    }
    rw->n_waiting_writers--;
    rw->n_active_writers++;
    pthread_mutex_unlock(&rw->lock_mutex);
}

// Function for a writer to release the lock
void writer_unlock(rwlock_t *rw) {
    pthread_mutex_lock(&rw->lock_mutex);
    rw->n_active_writers = 0;

    // Prioritize waking up writers if WRITERS priority, else prioritize readers
    if (rw->priority == WRITERS && rw->n_waiting_writers > 0) {
        pthread_cond_signal(&rw->writers_cond);
    } else {
        if (rw->n_waiting_readers > 0) {
            pthread_cond_broadcast(&rw->readers_cond);
        } else if (rw->n_waiting_writers > 0) {
            pthread_cond_signal(&rw->writers_cond);
        }
    }
    pthread_mutex_unlock(&rw->lock_mutex);
}
