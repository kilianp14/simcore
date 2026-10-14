#include "sim/sim.h"
#include "test.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Simulator creation */

/* Verifies returning a valid simulation. */
static bool test_sim_create_valid_output(void) {
    sim_t *sim = NULL;
    sim_time_t now = UINT64_MAX;

    CHECK(sim_create(&sim) == SIM_OK);
    CHECK(sim != NULL);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 0);
    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Verifies rejection of a null output pointer. */
static bool test_sim_create_validate_arguments(void) {
    CHECK(sim_create(NULL) == SIM_ERR_INVALID_ARGUMENT);
    return true;
}

/* Verifies that separate simulators maintain independent state. */
static bool test_sim_create_independent_instances(void) {
    sim_t *first = NULL;
    sim_t *second = NULL;
    sim_event_id_t event_id = 0;
    sim_time_t first_now = UINT64_MAX;
    sim_time_t second_now = UINT64_MAX;
    logging_context_t logging_context = {0};

    CHECK(sim_create(&first) == SIM_OK);
    CHECK(sim_create(&second) == SIM_OK);
    CHECK(sim_schedule_at(first, 5, logging_callback, &logging_context, NULL, &event_id) == SIM_OK);
    CHECK(sim_run(first) == SIM_OK);

    CHECK(sim_now(first, &first_now) == SIM_OK);
    CHECK(first_now == 5);
    CHECK(sim_now(second, &second_now) == SIM_OK);
    CHECK(second_now == 0);
    CHECK(sim_destroy(first) == SIM_OK);
    CHECK(sim_destroy(second) == SIM_OK);
    return true;
}

/* Verifies that a newly created simulator starts with fresh state. */
static bool test_sim_create_after_destroy(void) {
    sim_t *sim = NULL;
    sim_event_id_t event_id = 0;
    sim_time_t now = UINT64_MAX;
    logging_context_t logging_context = {0};

    CHECK(sim_create(&sim) == SIM_OK);
    CHECK(sim_schedule_at(sim, 5, logging_callback, &logging_context, NULL, &event_id) == SIM_OK);
    CHECK(sim_run(sim) == SIM_OK);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 5);
    CHECK(sim_destroy(sim) == SIM_OK);

    sim = NULL;
    CHECK(sim_create(&sim) == SIM_OK);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 0);
    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Simulator destruction */

/* Verifies rejection of a null simulator. */
static bool test_sim_destroy_validate_arguments(void) {
    CHECK(sim_destroy(NULL) == SIM_ERR_INVALID_ARGUMENT);
    return true;
}

/* Verifies that destruction cleans up each pending event's context without executing them. */
static bool test_sim_destroy_cleans_pending_events(void) {
    sim_t *sim = NULL;
    sim_event_id_t first_event_id = 0;
    sim_event_id_t second_event_id = 0;
    logging_context_t logging_context = {0};

    CHECK(sim_create(&sim) == SIM_OK);

    CHECK(
        sim_schedule_at(
            sim, 10, logging_callback, &logging_context, logging_cleanup_callback, &first_event_id
        ) == SIM_OK
    );
    CHECK(
        sim_schedule_at(
            sim, 20, logging_callback, &logging_context, logging_cleanup_callback, &second_event_id
        ) == SIM_OK
    );

    CHECK(sim_destroy(sim) == SIM_OK);
    CHECK(logging_context.callback_count == 0);
    CHECK(logging_context.cleanup_count == 2);

    return true;
}

/* Simulation time queries */

/* Verifies argument validation for sim_now(). */
static bool test_sim_now_validate_arguments(void) {
    sim_t *sim = NULL;
    sim_time_t now = UINT64_MAX;

    CHECK(sim_create(&sim) == SIM_OK);
    CHECK(sim_now(NULL, &now) == SIM_ERR_INVALID_ARGUMENT);
    CHECK(now == UINT64_MAX);
    CHECK(sim_now(sim, NULL) == SIM_ERR_INVALID_ARGUMENT);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 0);
    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

/* Status messages */

/* Verifies descriptions for all defined status codes and unknown values. */
static bool test_sim_status_string(void) {
    static const struct {
        sim_status_t status;
        const char *expected;
    } cases[] = {
        {SIM_OK, "Success"},
        {SIM_ERR_INVALID_ARGUMENT, "Invalid argument"},
        {SIM_ERR_OUT_OF_MEMORY, "Out of memory"},
        {SIM_ERR_NOT_FOUND, "Not found"},
        {SIM_ERR_TIME_IN_PAST, "Event time is in the past"},
        {SIM_ERR_TIME_OVERFLOW, "Simulation time overflow"},
        {SIM_ERR_ID_OVERFLOW, "Event ID overflow"},
        {SIM_ERR_CALLBACK, "Event callback failed"},
        {SIM_ERR_REENTRANT, "Reentrant operation"},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CHECK(strcmp(sim_status_string(cases[i].status), cases[i].expected) == 0);
    }

    CHECK(strcmp(sim_status_string(-1), "Unknown status") == 0);
    CHECK(strcmp(sim_status_string(999), "Unknown status") == 0);

    return true;
}

/* Lifecycle test suite. */

static const test_case_t tests[] = {
    /* Creation */
    TEST_CASE("sim_create_valid_output", test_sim_create_valid_output),
    TEST_CASE("sim_create_validate_arguments", test_sim_create_validate_arguments),
    TEST_CASE("sim_create_independent_instances", test_sim_create_independent_instances),
    TEST_CASE("sim_create_after_destroy", test_sim_create_after_destroy),

    /* Destruction */
    TEST_CASE("sim_destroy_validate_arguments", test_sim_destroy_validate_arguments),
    TEST_CASE("sim_destroy_cleans_pending_events", test_sim_destroy_cleans_pending_events),

    /* Time queries */
    TEST_CASE("sim_now_validate_arguments", test_sim_now_validate_arguments),

    /* Status messages */
    TEST_CASE("sim_status_string", test_sim_status_string),
};

test_suite_t test_suite_lifecycle(void) {
    return (test_suite_t){.tests = tests, .count = sizeof(tests) / sizeof(tests[0])};
}
