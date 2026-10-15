/**
 * @file event_queue.h
 * @brief Internal priority queue for pending simulation events.
 *
 * Private to the simulation kernel; not part of the public API.
 *
 * Events are ordered by ascending (time, event ID). The queue does
 * not allocate event IDs, manage simulation time, execute callbacks,
 * or invoke cleanup functions.
 *
 * The queue is single-threaded and not thread-safe.
 */

#ifndef SIMCORE_INTERNAL_EVENT_QUEUE_H
#define SIMCORE_INTERNAL_EVENT_QUEUE_H

#include "sim/sim.h"

#include <stdbool.h>
#include <stddef.h>

/* Event queue. */
typedef struct event_queue event_queue_t;

/**
 * Event record stored in the queue.
 *
 * The queue stores a shallow copy. User context ownership and event
 * lifecycle management remain the simulation kernel's responsibility.
 */
typedef struct {
    sim_event_id_t id;
    sim_time_t time;
    sim_callback_fn callback;
    void *user_context;
    sim_user_context_destroy_fn user_context_destroy;
} event_t;

/**
 * Create an empty event queue.
 *
 * @return A new queue, or NULL if allocation fails.
 */
event_queue_t *event_queue_create(void);

/**
 * Destroy a queue and release its internal storage.
 *
 * @param queue Queue to destroy; NULL is permitted.
 *
 * Pending events are discarded without executing callbacks or invoking
 * cleanup functions. The caller must handle pending-event cleanup first.
 */
void event_queue_destroy(event_queue_t *queue);

/**
 * Insert an event into the queue.
 *
 * @param queue Queue into which to insert the event.
 * @param event Event record to copy.
 * @return true on success, false if allocation fails.
 *
 * Preconditions:
 * - queue and event are non-NULL.
 * - event->id is non-zero and unique among pending events.
 * - event->callback is non-NULL.
 *
 * On failure, the queue's logical contents remain unchanged.
 * The queue does not take ownership of the user context.
 */
bool event_queue_insert(event_queue_t *queue, const event_t *event);

/**
 * Copy the earliest event without removing it.
 *
 * @param queue Queue to inspect.
 * @param[out] out Receives the event.
 * @return true if an event was returned, false if the queue is empty.
 *
 * Preconditions: queue and out are non-NULL.
 * If false is returned, *out remains unchanged.
 */
bool event_queue_peek(const event_queue_t *queue, event_t *out);

/**
 * Remove and return the earliest event.
 *
 * @param queue Queue from which to remove the event.
 * @param[out] out Receives the removed event.
 * @return true if an event was removed, false if the queue is empty.
 *
 * Preconditions: queue and out are non-NULL.
 * If false is returned, *out remains unchanged.
 *
 * The caller is responsible for completing the event's lifecycle.
 */
bool event_queue_pop(event_queue_t *queue, event_t *out);

/**
 * Remove a pending event by ID.
 *
 * @param queue Queue from which to remove the event.
 * @param id ID of the event to remove.
 * @param[out] out Receives the removed event.
 * @return true if the event was found and removed, false otherwise.
 *
 * Preconditions: queue and out are non-NULL.
 * If false is returned, *out remains unchanged.
 *
 * The caller is responsible for completing the event's lifecycle.
 */
bool event_queue_remove(event_queue_t *queue, sim_event_id_t id, event_t *out);

/**
 * Determine whether the queue is empty.
 *
 * @param queue Queue to inspect; must be non-NULL.
 */
bool event_queue_is_empty(const event_queue_t *queue);

/**
 * Get the number of pending events.
 *
 * @param queue Queue to inspect; must be non-NULL.
 */
size_t event_queue_size(const event_queue_t *queue);

#endif
