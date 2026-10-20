#include "event_queue.h"
#include "sim/sim.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define EVENT_QUEUE_INITIAL_CAPACITY 64

struct event_queue {
    event_t *heap;
    size_t size;
    size_t capacity;
};

/* Event order helper. */
static bool event_precedes(const event_t *a, const event_t *b) {
    if (a->time < b->time) {
        return true;
    }

    if (a->time > b->time) {
        return false;
    }

    return a->id < b->id;
}

/* Heap indexing helpers. */
static size_t parent(size_t i) { return (i - 1) / 2; }
static size_t left_child(size_t i) { return (2 * i + 1); }
static size_t right_child(size_t i) { return (2 * i + 2); }

/* Restore heap helpers. */
static void sift_down(event_queue_t *queue, size_t index) {
    while (index < queue->size / 2) {
        size_t left = left_child(index);
        size_t right = right_child(index);

        size_t child = left;
        if (right < queue->size && event_precedes(&queue->heap[right], &queue->heap[left])) {
            child = right;
        }

        if (!event_precedes(&queue->heap[child], &queue->heap[index])) {
            break;
        }

        event_t temp = queue->heap[index];
        queue->heap[index] = queue->heap[child];
        queue->heap[child] = temp;

        index = child;
    }
}

static void sift_up(event_queue_t *queue, size_t index) {
    while (index > 0) {
        size_t p = parent(index);

        if (!event_precedes(&queue->heap[index], &queue->heap[p])) {
            break;
        }

        event_t temp = queue->heap[p];
        queue->heap[p] = queue->heap[index];
        queue->heap[index] = temp;

        index = p;
    }
}

/* Re-allocation helper. */
static bool event_queue_grow(event_queue_t *queue) {
    const size_t max_capacity = SIZE_MAX / sizeof *queue->heap;

    if (queue->capacity >= max_capacity) {
        return false;
    }

    size_t new_capacity;
    if (queue->capacity > max_capacity / 2) {
        new_capacity = max_capacity;
    } else {
        new_capacity = queue->capacity * 2;
    }

    event_t *new_heap = realloc(queue->heap, new_capacity * sizeof *queue->heap);

    if (new_heap == NULL) {
        return false;
    }

    queue->heap = new_heap;
    queue->capacity = new_capacity;

    return true;
}

/* Event queue interface. */

event_queue_t *event_queue_create(void) {
    event_queue_t *queue = malloc(sizeof(event_queue_t));
    if (!queue) {
        return NULL;
    }

    queue->capacity = EVENT_QUEUE_INITIAL_CAPACITY;
    queue->size = 0;
    queue->heap = malloc(EVENT_QUEUE_INITIAL_CAPACITY * sizeof(event_t));
    if (queue->heap == NULL) {
        free(queue);
        return NULL;
    }

    return queue;
}

void event_queue_destroy(event_queue_t *queue) {
    if (!queue) {
        return;
    }
    free(queue->heap);
    free(queue);
    return;
}

bool event_queue_insert(event_queue_t *queue, const event_t *event) {
    if (queue->size == queue->capacity && !event_queue_grow(queue)) {
        return false;
    }

    queue->heap[queue->size] = *event;
    sift_up(queue, queue->size);
    queue->size++;

    return true;
}

bool event_queue_peek(const event_queue_t *queue, event_t *out) {
    if (!event_queue_size(queue)) {
        return false;
    }

    *out = queue->heap[0];
    return true;
}

bool event_queue_pop(event_queue_t *queue, event_t *out) {
    if (!event_queue_size(queue)) {
        return false;
    }

    event_t result = queue->heap[0];

    --queue->size;

    if (queue->size > 0) {
        queue->heap[0] = queue->heap[queue->size];
        sift_down(queue, 0);
    }

    *out = result;

    return true;
}

bool event_queue_remove(event_queue_t *queue, sim_event_id_t id, event_t *out) {
    size_t index = 0;

    /* Find the event. */
    while (index < queue->size && queue->heap[index].id != id) {
        index++;
    }
    if (index == queue->size) {
        return false;
    }

    event_t result = queue->heap[index];

    /* Fill the hole with the last event. */
    queue->size--;
    if (index < queue->size) {
        queue->heap[index] = queue->heap[queue->size];

        /* Restore the heap in the necessary direction. */
        if (index > 0 && event_precedes(&queue->heap[index], &queue->heap[parent(index)])) {
            sift_up(queue, index);
        } else {
            sift_down(queue, index);
        }
    }

    *out = result;
    return true;
}

bool event_queue_is_empty(const event_queue_t *queue) { return queue->size == 0; }

size_t event_queue_size(const event_queue_t *queue) { return queue->size; }
