#include "sim/sim.h"
#include "test.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Shared callback helpers */

/* Callback and context for scheduling event at same time. Includes logging. */
typedef struct {
    logging_context_t *logging_context;
    sim_status_t schedule_status;
    sim_event_id_t scheduled_id;
} schedule_now_context_t;

static sim_status_t
schedule_now_callback(sim_t *sim, sim_event_id_t event_id, sim_time_t now, void *user_context) {
    schedule_now_context_t *context = user_context;
    logging_context_t *logging_context = context->logging_context;

    logging_context->ids[logging_context->callback_count] = event_id;
    logging_context->times[logging_context->callback_count] = now;
    ++logging_context->callback_count;

    context->schedule_status =
        sim_schedule_at(sim, now, logging_callback, logging_context, NULL, &context->scheduled_id);

    return SIM_OK;
}

/* Failing callback. Includes logging. */
static sim_status_t
failing_callback(sim_t *sim, sim_event_id_t event_id, sim_time_t now, void *user_context) {
    logging_context_t *logging_context = user_context;
    (void)sim;

    logging_context->ids[logging_context->callback_count] = event_id;
    logging_context->times[logging_context->callback_count] = now;
    ++logging_context->callback_count;

    return SIM_ERR_INVALID_ARGUMENT;
}

/* Callback and context for recursive run call. Includes logging. */
typedef struct {
    logging_context_t *logging_context;
    sim_status_t nested_run_status;
    sim_status_t nested_run_until_status;
    sim_status_t nested_destroy_status;
} recursive_run_context_t;

static sim_status_t
recursive_run_callback(sim_t *sim, sim_event_id_t event_id, sim_time_t now, void *user_context) {
    recursive_run_context_t *context = user_context;
    logging_context_t *logging_context = context->logging_context;

    logging_context->ids[logging_context->callback_count] = event_id;
    logging_context->times[logging_context->callback_count] = now;
    ++logging_context->callback_count;

    context->nested_run_status = sim_run(sim);
    context->nested_run_until_status = sim_run_until(sim, now);
    context->nested_destroy_status = sim_destroy(sim);

    return SIM_OK;
}

/* sim_run execution */

/* Verifies execution order and that callback-scheduled events run in the same sim_run(). */
static bool test_sim_run_callback_schedules(void) {
    sim_t *sim = NULL;
    sim_event_id_t event_ids[3] = {0};
    logging_context_t logging_context = {0};
    schedule_now_context_t context = {
        .logging_context = &logging_context,
        .schedule_status = SIM_ERR_INVALID_ARGUMENT,
        .scheduled_id = 0,
    };

    CHECK(sim_create(&sim) == SIM_OK);

    /* The event at time 5 schedules another event at time 5. */
    CHECK(
        sim_schedule_at(sim, 10, logging_callback, &logging_context, NULL, &event_ids[0]) == SIM_OK
    );
    CHECK(sim_schedule_at(sim, 5, schedule_now_callback, &context, NULL, &event_ids[1]) == SIM_OK);
    CHECK(
        sim_schedule_at(sim, 5, logging_callback, &logging_context, NULL, &event_ids[2]) == SIM_OK
    );

    CHECK(sim_run(sim) == SIM_OK);

    CHECK(context.schedule_status == SIM_OK);
    CHECK(context.scheduled_id != 0);
    CHECK(logging_context.callback_count == 4);

    /* Same-time events execute by ID, including the event scheduled by the callback. */
    CHECK(logging_context.ids[0] == event_ids[1]);
    CHECK(logging_context.ids[1] == event_ids[2]);
    CHECK(logging_context.ids[2] == context.scheduled_id);
    CHECK(logging_context.ids[3] == event_ids[0]);

    CHECK(logging_context.times[0] == 5);
    CHECK(logging_context.times[1] == 5);
    CHECK(logging_context.times[2] == 5);
    CHECK(logging_context.times[3] == 10);

    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Verifies argument validation for sim_run. */
static bool test_sim_run_validate_arguments(void) {
    CHECK(sim_run(NULL) == SIM_ERR_INVALID_ARGUMENT);

    return true;
}

/* Verifies that sim_run() leaves time as is with empty queue. */
static bool test_sim_run_empty_queue(void) {
    sim_t *sim = NULL;
    sim_time_t now = UINT64_MAX;

    CHECK(sim_create(&sim) == SIM_OK);

    CHECK(sim_run(sim) == SIM_OK);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 0);

    CHECK(sim_run_until(sim, 10) == SIM_OK);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 10);

    CHECK(sim_run(sim) == SIM_OK);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 10);

    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Verifies that callback failure stops sim_run() and leaves later events queued. */
static bool test_sim_run_callback_failure(void) {
    sim_t *sim = NULL;
    sim_event_id_t event_ids[3] = {0};
    sim_time_t now = UINT64_MAX;
    logging_context_t logging_context = {0};

    CHECK(sim_create(&sim) == SIM_OK);

    CHECK(
        sim_schedule_at(sim, 5, logging_callback, &logging_context, NULL, &event_ids[0]) == SIM_OK
    );
    CHECK(
        sim_schedule_at(
            sim, 10, failing_callback, &logging_context, logging_cleanup_callback, &event_ids[1]
        ) == SIM_OK
    );
    CHECK(
        sim_schedule_at(sim, 15, logging_callback, &logging_context, NULL, &event_ids[2]) == SIM_OK
    );

    CHECK(sim_run(sim) == SIM_ERR_CALLBACK);

    /* The failing event is consumed and cleaned up; the later event remains pending. */
    CHECK(logging_context.callback_count == 2);
    CHECK(logging_context.ids[0] == event_ids[0]);
    CHECK(logging_context.times[0] == 5);
    CHECK(logging_context.ids[1] == event_ids[1]);
    CHECK(logging_context.times[1] == 10);
    CHECK(logging_context.cleanup_count == 1);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 10);

    /* A subsequent run can process the event that remained queued. */
    CHECK(sim_run(sim) == SIM_OK);
    CHECK(logging_context.callback_count == 3);
    CHECK(logging_context.ids[2] == event_ids[2]);
    CHECK(logging_context.times[2] == 15);
    CHECK(logging_context.cleanup_count == 1);

    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Verifies rejection of recursive run/destroy  during sim_run without stopping simulation. */
static bool test_sim_run_recursive_call(void) {
    sim_t *sim = NULL;
    sim_event_id_t first_id = 0;
    sim_event_id_t second_id = 0;
    logging_context_t logging_context = {0};
    recursive_run_context_t context = {
        .logging_context = &logging_context,
        .nested_run_status = SIM_OK,
        .nested_run_until_status = SIM_OK,
        .nested_destroy_status = SIM_OK,
    };

    CHECK(sim_create(&sim) == SIM_OK);

    CHECK(sim_schedule_at(sim, 5, recursive_run_callback, &context, NULL, &first_id) == SIM_OK);
    CHECK(sim_schedule_at(sim, 10, logging_callback, &logging_context, NULL, &second_id) == SIM_OK);

    CHECK(sim_run(sim) == SIM_OK);

    CHECK(context.nested_run_status == SIM_ERR_REENTRANT);
    CHECK(context.nested_run_until_status == SIM_ERR_REENTRANT);
    CHECK(context.nested_destroy_status == SIM_ERR_REENTRANT);

    /* The outer run continues to process the remaining event. */
    CHECK(logging_context.callback_count == 2);
    CHECK(logging_context.ids[0] == first_id);
    CHECK(logging_context.times[0] == 5);
    CHECK(logging_context.ids[1] == second_id);
    CHECK(logging_context.times[1] == 10);

    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* sim_run_until execution */

/* Verifies that sim_run_until() processes events at the inclusive time limit. */
static bool test_sim_run_until_callback_schedules(void) {
    sim_t *sim = NULL;
    sim_event_id_t event_ids[3] = {0};
    sim_time_t now = UINT64_MAX;
    logging_context_t logging_context = {0};
    schedule_now_context_t context = {
        .logging_context = &logging_context,
        .schedule_status = SIM_ERR_INVALID_ARGUMENT,
        .scheduled_id = 0,
    };

    CHECK(sim_create(&sim) == SIM_OK);

    /* The event at time 5 schedules another event at time 5. */
    CHECK(
        sim_schedule_at(sim, 10, logging_callback, &logging_context, NULL, &event_ids[0]) == SIM_OK
    );
    CHECK(sim_schedule_at(sim, 5, schedule_now_callback, &context, NULL, &event_ids[1]) == SIM_OK);
    CHECK(
        sim_schedule_at(sim, 5, logging_callback, &logging_context, NULL, &event_ids[2]) == SIM_OK
    );

    CHECK(sim_run_until(sim, 9) == SIM_OK);

    CHECK(context.schedule_status == SIM_OK);
    CHECK(context.scheduled_id != 0);

    /* Same-time events execute by ID, including the event scheduled by the callback. */
    CHECK(logging_context.ids[0] == event_ids[1]);
    CHECK(logging_context.ids[1] == event_ids[2]);
    CHECK(logging_context.ids[2] == context.scheduled_id);

    /* Only events at or before the limit execute. */
    CHECK(logging_context.callback_count == 3);
    CHECK(logging_context.times[0] == 5);
    CHECK(logging_context.times[1] == 5);
    CHECK(logging_context.times[2] == 5);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 9);

    /* Remaining events are untouched and until time is included. */
    CHECK(sim_run_until(sim, 10) == SIM_OK);
    CHECK(logging_context.callback_count == 4);
    CHECK(logging_context.ids[3] == event_ids[0]);
    CHECK(logging_context.times[3] == 10);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 10);

    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Verifies argument validation for sim_run_until. */
static bool test_sim_run_until_validate_arguments(void) {
    CHECK(sim_run_until(NULL, 0) == SIM_ERR_INVALID_ARGUMENT);

    return true;
}

/* Verifies that sim_run_until() advances time with empty queue. */
static bool test_sim_run_until_empty_queue(void) {
    sim_t *sim = NULL;
    sim_time_t now = UINT64_MAX;

    CHECK(sim_create(&sim) == SIM_OK);

    CHECK(sim_run_until(sim, 10) == SIM_OK);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 10);

    CHECK(sim_run_until(sim, 12) == SIM_OK);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 12);

    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Verifies that a past run limit fails without changing time or the pending queue. */
static bool test_sim_run_until_past_time(void) {
    sim_t *sim = NULL;
    sim_event_id_t event_id = 0;
    sim_time_t now = UINT64_MAX;
    logging_context_t logging_context = {0};

    CHECK(sim_create(&sim) == SIM_OK);
    CHECK(sim_run_until(sim, 10) == SIM_OK);

    CHECK(sim_schedule_at(sim, 15, logging_callback, &logging_context, NULL, &event_id) == SIM_OK);

    CHECK(sim_run_until(sim, 9) == SIM_ERR_TIME_IN_PAST);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 10);
    CHECK(logging_context.callback_count == 0);

    CHECK(sim_run_until(sim, 20) == SIM_OK);
    CHECK(logging_context.callback_count == 1);
    CHECK(logging_context.ids[0] == event_id);
    CHECK(logging_context.times[0] == 15);

    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Verifies that callback failure stops sim_run_until() at the failed event. */
static bool test_sim_run_until_callback_failure(void) {
    sim_t *sim = NULL;
    sim_event_id_t event_ids[3] = {0};
    sim_time_t now = UINT64_MAX;
    logging_context_t logging_context = {0};

    CHECK(sim_create(&sim) == SIM_OK);

    CHECK(
        sim_schedule_at(sim, 5, logging_callback, &logging_context, NULL, &event_ids[0]) == SIM_OK
    );
    CHECK(
        sim_schedule_at(
            sim, 10, failing_callback, &logging_context, logging_cleanup_callback, &event_ids[1]
        ) == SIM_OK
    );
    CHECK(
        sim_schedule_at(sim, 15, logging_callback, &logging_context, NULL, &event_ids[2]) == SIM_OK
    );

    CHECK(sim_run_until(sim, 20) == SIM_ERR_CALLBACK);

    /* The failing event is consumed and cleaned up; the later event remains pending. */
    CHECK(logging_context.callback_count == 2);
    CHECK(logging_context.ids[0] == event_ids[0]);
    CHECK(logging_context.times[0] == 5);
    CHECK(logging_context.ids[1] == event_ids[1]);
    CHECK(logging_context.times[1] == 10);
    CHECK(logging_context.cleanup_count == 1);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 10);

    /* The remaining event executes successfully on the next run. */
    CHECK(sim_run_until(sim, 20) == SIM_OK);
    CHECK(logging_context.callback_count == 3);
    CHECK(logging_context.ids[2] == event_ids[2]);
    CHECK(logging_context.times[2] == 15);
    CHECK(logging_context.cleanup_count == 1);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 20);

    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Verifies rejection of recursive run/destroy  during sim_run_until without stopping simulation. */
static bool test_sim_run_until_recursive_call(void) {
    sim_t *sim = NULL;
    sim_event_id_t first_id = 0;
    sim_event_id_t second_id = 0;
    logging_context_t logging_context = {0};
    recursive_run_context_t context = {
        .logging_context = &logging_context,
        .nested_run_status = SIM_OK,
        .nested_run_until_status = SIM_OK,
        .nested_destroy_status = SIM_OK,
    };

    CHECK(sim_create(&sim) == SIM_OK);

    CHECK(sim_schedule_at(sim, 5, recursive_run_callback, &context, NULL, &first_id) == SIM_OK);
    CHECK(sim_schedule_at(sim, 10, logging_callback, &logging_context, NULL, &second_id) == SIM_OK);

    CHECK(sim_run_until(sim, 10) == SIM_OK);

    CHECK(context.nested_run_status == SIM_ERR_REENTRANT);
    CHECK(context.nested_run_until_status == SIM_ERR_REENTRANT);
    CHECK(context.nested_destroy_status == SIM_ERR_REENTRANT);

    /* The outer run continues to process the remaining event. */
    CHECK(logging_context.callback_count == 2);
    CHECK(logging_context.ids[0] == first_id);
    CHECK(logging_context.times[0] == 5);
    CHECK(logging_context.ids[1] == second_id);
    CHECK(logging_context.times[1] == 10);

    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Execution test suite. */

static const test_case_t tests[] = {
    /* sim_run */
    TEST_CASE("sim_run_callback_schedules", test_sim_run_callback_schedules),
    TEST_CASE("sim_run_validate_arguments", test_sim_run_validate_arguments),
    TEST_CASE("sim_run_empty_queue", test_sim_run_empty_queue),
    TEST_CASE("sim_run_callback_failure", test_sim_run_callback_failure),
    TEST_CASE("sim_run_recursive_call", test_sim_run_recursive_call),

    /* sim_run_until */
    TEST_CASE("sim_run_until_callback_schedules", test_sim_run_until_callback_schedules),
    TEST_CASE("sim_run_until_validate_arguments", test_sim_run_until_validate_arguments),
    TEST_CASE("sim_run_until_empty_queue", test_sim_run_until_empty_queue),
    TEST_CASE("sim_run_until_past_time", test_sim_run_until_past_time),
    TEST_CASE("sim_run_until_callback_failure", test_sim_run_until_callback_failure),
    TEST_CASE("sim_run_until_recursive_call", test_sim_run_until_recursive_call),
};

test_suite_t test_suite_run(void) {
    return (test_suite_t){.tests = tests, .count = sizeof(tests) / sizeof(tests[0])};
}
