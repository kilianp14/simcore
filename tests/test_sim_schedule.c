#include "sim/sim.h"
#include "test.h"

#include <stdbool.h>
#include <stdlib.h>

/* Event scheduling */

/* Verifies chronological ordering, event-ID tie-breaking, and scheduling at the current time. */
static bool test_sim_schedule_at_orders_by_time_then_id(void) {
    sim_t *sim = NULL;
    sim_event_id_t scheduled_ids[4] = {0};
    logging_context_t logging_context = {0};

    CHECK(sim_create(&sim) == SIM_OK);

    /* Schedule out of chronological order, including events at time 0. */
    CHECK(
        sim_schedule_at(sim, 2, logging_callback, &logging_context, NULL, &scheduled_ids[0]) ==
        SIM_OK
    );
    CHECK(
        sim_schedule_at(sim, 0, logging_callback, &logging_context, NULL, &scheduled_ids[1]) ==
        SIM_OK
    );
    CHECK(
        sim_schedule_at(sim, 2, logging_callback, &logging_context, NULL, &scheduled_ids[2]) ==
        SIM_OK
    );
    CHECK(
        sim_schedule_at(sim, 0, logging_callback, &logging_context, NULL, &scheduled_ids[3]) ==
        SIM_OK
    );

    /* IDs follow successful scheduling order, not execution order. */
    CHECK(scheduled_ids[0] == 1);
    CHECK(scheduled_ids[1] == 2);
    CHECK(scheduled_ids[2] == 3);
    CHECK(scheduled_ids[3] == 4);

    CHECK(sim_run(sim) == SIM_OK);

    CHECK(logging_context.callback_count == 4);

    /* Earlier times execute first; ties are broken by ascending ID. */
    CHECK(logging_context.ids[0] == 2);
    CHECK(logging_context.ids[1] == 4);
    CHECK(logging_context.ids[2] == 1);
    CHECK(logging_context.ids[3] == 3);

    CHECK(logging_context.times[0] == 0);
    CHECK(logging_context.times[1] == 0);
    CHECK(logging_context.times[2] == 2);
    CHECK(logging_context.times[3] == 2);

    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Verifies that scheduling in the past fails without changing simulator state. */
static bool test_sim_schedule_at_past_time(void) {
    sim_t *sim = NULL;
    sim_event_id_t first_id = 0;
    sim_event_id_t failed_id = UINT64_MAX;
    sim_event_id_t second_id = 0;
    sim_time_t now = UINT64_MAX;
    logging_context_t logging_context = {0};

    CHECK(sim_create(&sim) == SIM_OK);
    CHECK(sim_run_until(sim, 5) == SIM_OK);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 5);

    /* Schedule a valid event to verify that a failed attempt preserves the queue. */
    CHECK(sim_schedule_at(sim, 8, logging_callback, &logging_context, NULL, &first_id) == SIM_OK);
    CHECK(first_id == 1);

    /* A past-time event must fail without consuming an ID or taking ownership. */
    CHECK(
        sim_schedule_at(
            sim, 4, logging_callback, &logging_context, logging_cleanup_callback, &failed_id
        ) == SIM_ERR_TIME_IN_PAST
    );
    CHECK(failed_id == 0);
    CHECK(logging_context.cleanup_count == 0);

    /* The failed call must not change the current state. */
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 5);

    /* The next successful event must receive the next consecutive ID. */
    CHECK(sim_schedule_at(sim, 5, logging_callback, &logging_context, NULL, &second_id) == SIM_OK);
    CHECK(second_id == 2);

    /* Both valid events should remain queued and execute exactly once. */
    CHECK(sim_run(sim) == SIM_OK);
    CHECK(logging_context.callback_count == 2);
    CHECK(logging_context.cleanup_count == 0);

    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Verifies argument validation for both scheduling functions. */
static bool test_sim_schedule_validate_arguments(void) {
    sim_t *sim = NULL;
    sim_event_id_t out_id = UINT64_MAX;
    sim_event_id_t first_id = 0;
    sim_event_id_t second_id = 0;
    logging_context_t logging_context = {0};

    CHECK(sim_create(&sim) == SIM_OK);

    /* sim_schedule_at: null simulator, callback, and output pointer. */
    CHECK(
        sim_schedule_at(NULL, 0, logging_callback, &logging_context, NULL, &out_id) ==
        SIM_ERR_INVALID_ARGUMENT
    );
    CHECK(out_id == 0);
    out_id = UINT64_MAX;
    CHECK(
        sim_schedule_at(sim, 0, NULL, &logging_context, NULL, &out_id) == SIM_ERR_INVALID_ARGUMENT
    );
    CHECK(out_id == 0);
    CHECK(
        sim_schedule_at(sim, 0, logging_callback, &logging_context, NULL, NULL) ==
        SIM_ERR_INVALID_ARGUMENT
    );

    /* sim_schedule_after: null simulator, callback, and output pointer. */
    out_id = UINT64_MAX;
    CHECK(
        sim_schedule_after(NULL, 0, logging_callback, &logging_context, NULL, &out_id) ==
        SIM_ERR_INVALID_ARGUMENT
    );
    CHECK(out_id == 0);
    out_id = UINT64_MAX;
    CHECK(
        sim_schedule_after(sim, 0, NULL, &logging_context, NULL, &out_id) ==
        SIM_ERR_INVALID_ARGUMENT
    );
    CHECK(out_id == 0);
    CHECK(
        sim_schedule_after(sim, 0, logging_callback, &logging_context, NULL, NULL) ==
        SIM_ERR_INVALID_ARGUMENT
    );

    /* Failed calls must not consume IDs or prevent subsequent scheduling. */
    CHECK(sim_schedule_at(sim, 0, logging_callback, &logging_context, NULL, &first_id) == SIM_OK);
    CHECK(first_id == 1);
    CHECK(
        sim_schedule_after(sim, 0, logging_callback, &logging_context, NULL, &second_id) == SIM_OK
    );
    CHECK(second_id == 2);
    CHECK(sim_run(sim) == SIM_OK);
    CHECK(logging_context.callback_count == 2);
    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Verifies that delays are relative to the current simulation time. */
static bool test_sim_schedule_after_uses_current_time(void) {
    sim_t *sim = NULL;
    sim_event_id_t event_ids[3] = {0};
    logging_context_t logging_context = {0};

    CHECK(sim_create(&sim) == SIM_OK);
    CHECK(sim_run_until(sim, 10) == SIM_OK);

    /* Zero delay targets time 10; a delay of 3 targets time 13. */
    CHECK(
        sim_schedule_after(sim, 0, logging_callback, &logging_context, NULL, &event_ids[0]) ==
        SIM_OK
    );
    CHECK(
        sim_schedule_after(sim, 3, logging_callback, &logging_context, NULL, &event_ids[1]) ==
        SIM_OK
    );

    /* Execute the time-10 event and advance the clock to 12. */
    CHECK(sim_run_until(sim, 12) == SIM_OK);
    CHECK(logging_context.callback_count == 1);
    CHECK(logging_context.ids[0] == event_ids[0]);
    CHECK(logging_context.times[0] == 10);

    /* The new delay is measured from time 12, so this event targets time 14. */
    CHECK(
        sim_schedule_after(sim, 2, logging_callback, &logging_context, NULL, &event_ids[2]) ==
        SIM_OK
    );
    CHECK(sim_run(sim) == SIM_OK);

    CHECK(logging_context.callback_count == 3);
    CHECK(logging_context.ids[1] == event_ids[1]);
    CHECK(logging_context.times[1] == 13);
    CHECK(logging_context.ids[2] == event_ids[2]);
    CHECK(logging_context.times[2] == 14);

    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Verifies time-overflow detection and scheduling at UINT64_MAX. */
static bool test_sim_schedule_after_time_overflow(void) {
    sim_t *sim = NULL;
    sim_event_id_t failed_id = UINT64_MAX;
    sim_event_id_t first_id = 0;
    sim_event_id_t second_id = 0;
    sim_time_t now = 0;
    logging_context_t logging_context = {0};

    CHECK(sim_create(&sim) == SIM_OK);
    CHECK(sim_run_until(sim, UINT64_MAX - 1) == SIM_OK);

    /* Adding two ticks would overflow the simulation-time range. */
    CHECK(
        sim_schedule_after(
            sim, 2, logging_callback, &logging_context, logging_cleanup_callback, &failed_id
        ) == SIM_ERR_TIME_OVERFLOW
    );
    CHECK(failed_id == 0);
    CHECK(logging_context.cleanup_count == 0);

    /* The failed attempt must leave the current time unchanged. */
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == UINT64_MAX - 1);

    /* Events can succeed at the maximum representable time. */
    CHECK(
        sim_schedule_after(sim, 1, logging_callback, &logging_context, NULL, &first_id) == SIM_OK
    );
    CHECK(
        sim_schedule_at(sim, UINT64_MAX, logging_callback, &logging_context, NULL, &second_id) ==
        SIM_OK
    );

    CHECK(sim_run(sim) == SIM_OK);
    CHECK(logging_context.callback_count == 2);
    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Event cancelling */

/* Verifies that cancelling a pending event prevents it from executing. */
static bool test_sim_cancel_remove_pending_event(void) {
    sim_t *sim = NULL;
    sim_event_id_t cancelled_id = 0;
    sim_event_id_t remaining_id = 0;
    logging_context_t logging_context = {0};

    CHECK(sim_create(&sim) == SIM_OK);

    CHECK(
        sim_schedule_at(sim, 10, logging_callback, &logging_context, NULL, &cancelled_id) == SIM_OK
    );
    CHECK(
        sim_schedule_at(sim, 20, logging_callback, &logging_context, NULL, &remaining_id) == SIM_OK
    );

    CHECK(sim_cancel(sim, cancelled_id) == SIM_OK);
    CHECK(sim_run(sim) == SIM_OK);
    CHECK(logging_context.callback_count == 1);

    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Verifies that cancellation rejects a null simulator. */
static bool test_sim_cancel_validate_arguments(void) {
    CHECK(sim_cancel(NULL, 1) == SIM_ERR_INVALID_ARGUMENT);
    return true;
}

/* Verifies cancellation returns not found for unknown and completed events. */
static bool test_sim_cancel_unknown_and_completed_events(void) {
    sim_t *sim = NULL;
    sim_event_id_t event_id = 0;
    logging_context_t logging_context = {0};

    CHECK(sim_create(&sim) == SIM_OK);

    CHECK(sim_cancel(sim, 67) == SIM_ERR_NOT_FOUND);

    CHECK(sim_schedule_at(sim, 5, logging_callback, &logging_context, NULL, &event_id) == SIM_OK);
    CHECK(sim_run(sim) == SIM_OK);
    CHECK(logging_context.callback_count == 1);

    CHECK(sim_cancel(sim, event_id) == SIM_ERR_NOT_FOUND);

    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Verifies synchronous cleanup and exactly-once cleanup after cancellation. */
static bool test_sim_cancel_synchronous_exactly_once(void) {
    sim_t *sim = NULL;
    sim_event_id_t event_id = 0;
    logging_context_t logging_context = {0};

    CHECK(sim_create(&sim) == SIM_OK);

    CHECK(
        sim_schedule_at(
            sim, 10, logging_callback, &logging_context, logging_cleanup_callback, &event_id
        ) == SIM_OK
    );

    CHECK(sim_cancel(sim, event_id) == SIM_OK);

    /* Cleanup must have completed before sim_cancel() returns. */
    CHECK(logging_context.cleanup_count == 1);

    CHECK(sim_cancel(sim, event_id) == SIM_ERR_NOT_FOUND);
    CHECK(logging_context.cleanup_count == 1);

    /* The cancelled callback must not execute, and cleanup must not repeat. */
    CHECK(sim_run(sim) == SIM_OK);
    CHECK(logging_context.callback_count == 0);

    CHECK(sim_destroy(sim) == SIM_OK);
    CHECK(logging_context.cleanup_count == 1);

    return true;
}

/* Scheduling test suite. */

static const test_case_t tests[] = {
    /* Scheduling */
    TEST_CASE(
        "sim_schedule_at_orders_by_time_then_id", test_sim_schedule_at_orders_by_time_then_id
    ),
    TEST_CASE("sim_schedule_at_past_time", test_sim_schedule_at_past_time),
    TEST_CASE("sim_schedule_validate_arguments", test_sim_schedule_validate_arguments),
    TEST_CASE("sim_schedule_after_uses_current_time", test_sim_schedule_after_uses_current_time),
    TEST_CASE("sim_schedule_after_time_overflow", test_sim_schedule_after_time_overflow),

    /* Cancelling */
    TEST_CASE("sim_cancel_remove_pending_event", test_sim_cancel_remove_pending_event),
    TEST_CASE("sim_cancel_validate_arguments", test_sim_cancel_validate_arguments),
    TEST_CASE(
        "sim_cancel_unknown_and_completed_events", test_sim_cancel_unknown_and_completed_events
    ),
    TEST_CASE("sim_cancel_synchronous_exactly_once", test_sim_cancel_synchronous_exactly_once),
};

int main(int argc, char **argv) {
    test_suite_t suite = {
        .name = "Integration: Simulation schedule",
        .tests = tests,
        .count = sizeof(tests) / sizeof(tests[0])
    };
    return test_run(&suite, argc, argv);
}
