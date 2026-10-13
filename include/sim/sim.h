/**
 * @file sim.h
 * @brief Deterministic discrete-event simulation kernel.
 *
 * Simulation time is an unsigned tick count starting at 0. Events execute
 * in ascending (time, event ID) order. The API is single-threaded and
 * not thread-safe.
 */

#ifndef SIM_H
#define SIM_H

#include <stdint.h>

/** Simulation instance. */
typedef struct sim sim_t;

/** Simulation time. */
typedef uint64_t sim_time_t;

/**
 * Event identifier, unique within a simulator.
 * ID 0 is reserved as invalid. IDs start at 1, increase monotonically
 * in successful scheduling order, and are never reused. UINT64_MAX
 * may be assigned; subsequent scheduling attempts fail with
 * SIM_ERR_ID_OVERFLOW. Failed scheduling attempts do not consume IDs.
 */
typedef uint64_t sim_event_id_t;

/** Function return status. */
typedef int32_t sim_status_t;

/** Status codes returned by the API. */
enum {
    SIM_OK = 0,
    SIM_ERR_INVALID_ARGUMENT = 1,
    SIM_ERR_OUT_OF_MEMORY = 2,
    SIM_ERR_NOT_FOUND = 3,
    SIM_ERR_TIME_IN_PAST = 4,
    SIM_ERR_TIME_OVERFLOW = 5,
    SIM_ERR_ID_OVERFLOW = 6,
    SIM_ERR_CALLBACK = 7,
    SIM_ERR_REENTRANT = 8
};

/**
 * Event callback.
 *
 * @param sim       Owning simulator.
 * @param event_id  ID of the executing event.
 * @param now       Current simulation time, equal to the event's time.
 * @param user_context User-provided pointer.
 * @return SIM_OK on success. Any other value stops the current run and
 *         causes it to return SIM_ERR_CALLBACK.
 *
 * The event is removed from the queue before its callback executes.
 * Callbacks may schedule or cancel events, but may not recursively run
 * or destroy the simulator.
 */
typedef sim_status_t (*sim_callback_fn)(
    sim_t *sim, sim_event_id_t event_id, sim_time_t now, void *user_context
);

/**
 * Optional user-data cleanup callback.
 *
 * @param user_context User-provided pointer.
 *
 * Called exactly once for each successfully scheduled event with a
 * non-NULL cleanup function, after its callback completes, when
 * cancelled, or when discarded during destruction.
 *
 * Cleanup runs synchronously, before the next event callback or
 * before the triggering API call returns. During destruction,
 * pending events are cleaned up in ascending event-ID order.
 *
 * A cleanup callback must not call any public API function on its
 * owning simulator, including read-only functions such as sim_now().
 * It must not attempt to destroy the owning simulator.
 *
 * Violating this restriction results in undefined behavior.
 */
typedef void (*sim_user_context_destroy_fn)(void *user_context);

/**
 * Creates a simulator.
 *
 * @param[out] out Receives the new simulator.
 * @return SIM_OK, SIM_ERR_INVALID_ARGUMENT, or SIM_ERR_OUT_OF_MEMORY.
 */
sim_status_t sim_create(sim_t **out);

/**
 * Destroy a simulator and discard all pending events.
 *
 * @param sim Simulator to destroy; must not be NULL.
 * @return SIM_OK, SIM_ERR_INVALID_ARGUMENT, or SIM_ERR_REENTRANT.
 *
 * Returns SIM_ERR_REENTRANT without destroying the simulator if a run
 * is active. On success, sim becomes invalid and must not be reused.
 */
sim_status_t sim_destroy(sim_t *sim);

/**
 * Get the current simulation time.
 *
 * @param sim Simulator.
 * @param[out] out Receives the current time.
 * @return SIM_OK or SIM_ERR_INVALID_ARGUMENT.
 */
sim_status_t sim_now(const sim_t *sim, sim_time_t *out);

/**
 * Schedule an event at an absolute simulation time.
 *
 * @param sim Simulator.
 * @param time Event time; must be >= current simulation time.
 * @param callback Function to execute; must not be NULL.
 * @param user_context Pointer passed to the callback.
 * @param user_context_destroy Optional cleanup function; may be NULL.
 * @param[out] out_id Receives the assigned event ID.
 * @return SIM_OK, SIM_ERR_INVALID_ARGUMENT, SIM_ERR_TIME_IN_PAST,
 *         SIM_ERR_ID_OVERFLOW, or SIM_ERR_OUT_OF_MEMORY.
 *
 * Events at the same time execute in ascending event-ID order.
 * Scheduling at the current time is allowed.
 *
 * If user_context_destroy is non-NULL, ownership of user_context transfers
 * only on success, and cleanup runs exactly once. On failure, ownership
 * remains with the caller and cleanup is not called.
 *
 * On failure, *out_id is set to 0 when out_id is valid. Failed calls
 * do not modify simulator state or consume an event ID.
 */
sim_status_t sim_schedule_at(
    sim_t *sim,
    sim_time_t time,
    sim_callback_fn callback,
    void *user_context,
    sim_user_context_destroy_fn user_context_destroy,
    sim_event_id_t *out_id
);

/**
 * Schedule an event after a delay from the current simulation time.
 *
 * @param sim Simulator.
 * @param delay Delay in ticks; zero is allowed.
 * @param callback Function to execute; must not be NULL.
 * @param user_context Pointer passed to the callback.
 * @param user_context_destroy Optional cleanup function; may be NULL.
 * @param[out] out_id Receives the assigned event ID.
 * @return SIM_OK, SIM_ERR_INVALID_ARGUMENT, SIM_ERR_TIME_OVERFLOW,
 *         SIM_ERR_ID_OVERFLOW, or SIM_ERR_OUT_OF_MEMORY.
 *
 * Has the same ordering and ownership semantics as sim_schedule_at().
 * Fails with SIM_ERR_TIME_OVERFLOW if current time + delay exceeds
 * UINT64_MAX.
 */
sim_status_t sim_schedule_after(
    sim_t *sim,
    sim_time_t delay,
    sim_callback_fn callback,
    void *user_context,
    sim_user_context_destroy_fn user_context_destroy,
    sim_event_id_t *out_id
);

/**
 * Cancel a pending event.
 *
 * @param sim Simulator.
 * @param event_id Event to cancel.
 * @return SIM_OK, SIM_ERR_INVALID_ARGUMENT, or SIM_ERR_NOT_FOUND.
 *
 * A cancelled event will not execute. Its cleanup callback, if any,
 * runs before this function returns. Executing or completed events
 * cannot be cancelled.
 */
sim_status_t sim_cancel(sim_t *sim, sim_event_id_t event_id);

/**
 * Execute events until the event queue is empty.
 *
 * @param sim Simulator.
 * @return SIM_OK, SIM_ERR_INVALID_ARGUMENT, SIM_ERR_CALLBACK,
 *         or SIM_ERR_REENTRANT.
 *
 * Events scheduled by callbacks are processed during the same run.
 * Time advances to each executed event's time and remains unchanged
 * if the queue is initially empty.
 *
 * If a callback fails, that event is consumed, its cleanup runs,
 * remaining events stay queued, and time remains at the failed
 * event's time.
 */
sim_status_t sim_run(sim_t *sim);

/**
 * Execute all events whose time is <= until.
 *
 * @param sim Simulator.
 * @param until Inclusive time limit; must be >= current time.
 * @return SIM_OK, SIM_ERR_INVALID_ARGUMENT, SIM_ERR_TIME_IN_PAST,
 *         SIM_ERR_CALLBACK, or SIM_ERR_REENTRANT.
 *
 * Events scheduled by callbacks are processed during the same run
 * if their times are <= until. On success, time advances to until,
 * even if no event occurs there. If a callback fails, time remains
 * at the failed event's time.
 *
 * If until is in the past, the call has no effect.
 */
sim_status_t sim_run_until(sim_t *sim, sim_time_t until);

/**
 * Get a human-readable description of a status code.
 *
 * @param status Status code.
 * @return Pointer to a static string valid for the lifetime of the
 *         library. Unknown codes return "Unknown status".
 */
const char *sim_status_string(sim_status_t status);

#endif
