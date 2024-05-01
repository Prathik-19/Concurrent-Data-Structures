#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "queue.h"

// Define the structure for the queue
typedef struct queue {
    void **items; // Array to hold queue elements
    int capacity; // Maximum capacity of the queue
    int cnt; // Current number of elements in the queue
    int head_idx; // Index of the first element in the queue
    int tail_idx; // Index where the next element will be added
    pthread_mutex_t lock; // Mutex for thread safety
    pthread_cond_t not_empty_cond; // Condition variable to signal when queue is not empty
    pthread_cond_t not_full_cond; // Condition variable to signal when queue is not full
} queue_t;

// Function to create a new queue with specified capacity
queue_t *queue_new(int capacity) {
    if (capacity < 1) {
        return NULL;
    }

    // Allocate memory for the queue
    queue_t *q = malloc(sizeof(queue_t));
    if (!q) {
        return NULL;
    }

    // Allocate memory for the array of items in the queue
    q->items = malloc(sizeof(void *) * capacity);
    if (!q->items) {
        free(q);
        return NULL;
    }

    // Initialize queue parameters
    q->capacity = capacity;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty_cond, NULL);
    pthread_cond_init(&q->not_full_cond, NULL);
    q->head_idx = 0;
    q->tail_idx = 0;
    q->cnt = 0;

    return q;
}

// Function to delete a queue and free its memory
void queue_delete(queue_t **q) {
    if (q && *q) {
        pthread_mutex_lock(&(*q)->lock);
        pthread_mutex_destroy(&(*q)->lock);
        pthread_cond_destroy(&(*q)->not_empty_cond);
        pthread_cond_destroy(&(*q)->not_full_cond);

        // Free memory allocated for queue items and the queue itself
        free((*q)->items);
        free(*q);

        *q = NULL;
    }
}

// Function to push an element onto the queue
bool queue_push(queue_t *q, void *elem) {
    if (!q) {
        return false;
    }

    pthread_mutex_lock(&q->lock);

    // Wait while the queue is full
    while (q->capacity == q->cnt) {
        pthread_cond_wait(&q->not_full_cond, &q->lock);
    }

    // Add the element to the queue
    q->items[q->tail_idx] = elem;
    q->tail_idx = q->tail_idx + 1;
    q->tail_idx = q->tail_idx % q->capacity;
    q->cnt++;

    // Signal that the queue is not empty
    pthread_cond_signal(&q->not_empty_cond);
    pthread_mutex_unlock(&q->lock);
    return true;
}

// Function to pop an element from the queue
bool queue_pop(queue_t *q, void **elem) {
    if (!q) {
        return false;
    } else if (!elem) {
        return false;
    }

    pthread_mutex_lock(&q->lock);

    // Wait while the queue is empty
    while (q->cnt == 0) {
        pthread_cond_wait(&q->not_empty_cond, &q->lock);
    }

    // Retrieve the element from the queue
    *elem = q->items[q->head_idx];
    q->head_idx = q->head_idx + 1;
    q->head_idx = q->head_idx % q->capacity;
    q->cnt--;

    // Signal that the queue is not full
    pthread_cond_signal(&q->not_full_cond);
    pthread_mutex_unlock(&q->lock);
    return true;
}
