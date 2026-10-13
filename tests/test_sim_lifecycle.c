#include "sim/sim.h"
#include "test.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

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

static bool test_sim_create_null_output(void) {
    CHECK(sim_create(NULL) == SIM_ERR_INVALID_ARGUMENT);
    return true;
}

static bool test_sim_create_independent_instances(void) {
    sim_t *first = NULL;
    sim_t *second = NULL;
    sim_event_id_t event_id = 0;
    sim_time_t first_now = UINT64_MAX;
    sim_time_t second_now = UINT64_MAX;
    int callback_count = 0;

    CHECK(sim_create(&first) == SIM_OK);
    CHECK(sim_create(&second) == SIM_OK);
    CHECK(
        sim_schedule_at(first, 5, simple_count_callback, &callback_count, NULL, &event_id) == SIM_OK
    );
    CHECK(sim_run(first) == SIM_OK);
    CHECK(callback_count == 1);
    CHECK(sim_now(first, &first_now) == SIM_OK);
    CHECK(sim_now(second, &second_now) == SIM_OK);
    CHECK(first_now == 5);
    CHECK(second_now == 0);
    CHECK(sim_destroy(first) == SIM_OK);
    CHECK(sim_destroy(second) == SIM_OK);
    return true;
}

static bool test_sim_create_after_destroy(void) {
    sim_t *sim = NULL;
    sim_time_t now = UINT64_MAX;
    sim_event_id_t event_id = 0;
    int callback_count = 0;

    CHECK(sim_create(&sim) == SIM_OK);
    CHECK(
        sim_schedule_at(sim, 5, simple_count_callback, &callback_count, NULL, &event_id) == SIM_OK
    );
    CHECK(sim_run(sim) == SIM_OK);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 5);
    CHECK(callback_count == 1);
    CHECK(sim_destroy(sim) == SIM_OK);

    sim = NULL;
    CHECK(sim_create(&sim) == SIM_OK);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 0);
    CHECK(sim_destroy(sim) == SIM_OK);
    return true;
}

static bool test_sim_destroy_null(void) {
    CHECK(sim_destroy(NULL) == SIM_ERR_INVALID_ARGUMENT);
    return true;
}

static bool test_sim_destroy_discards_pending_events(void) {
    sim_t *sim = NULL;
    sim_event_id_t first_id = 0;
    sim_event_id_t second_id = 0;
    int callback_count = 0;

    CHECK(sim_create(&sim) == SIM_OK);
    CHECK(
        sim_schedule_at(sim, 10, simple_count_callback, &callback_count, NULL, &first_id) == SIM_OK
    );
    CHECK(
        sim_schedule_at(sim, 20, simple_count_callback, &callback_count, NULL, &second_id) == SIM_OK
    );
    CHECK(sim_destroy(sim) == SIM_OK);
    CHECK(callback_count == 0);
    return true;
}

static bool test_sim_destroy_cleans_pending_events(void) {
    sim_t *sim = NULL;
    sim_event_id_t id;
    int total = 0;
    int destroy_count = 0;
    owned_context_t *ctx;

    CHECK(sim_create(&sim) == SIM_OK);

    for (int i = 1; i <= 2; ++i) {
        ctx = malloc(sizeof(*ctx));
        if (ctx == NULL) {
            sim_destroy(sim);
            return false;
        }

        *ctx = (owned_context_t){.value = i, .total = &total, .destroy_count = &destroy_count};

        if (sim_schedule_at(
                sim, (sim_time_t)(i * 10), owned_callback, ctx, owned_context_destroy, &id
            ) != SIM_OK) {
            free(ctx); /* Ownership transfers only on success. */
            sim_destroy(sim);
            return false;
        }
    }

    CHECK(sim_destroy(sim) == SIM_OK);
    CHECK(total == 0);
    CHECK(destroy_count == 2);

    return true;
}

static bool test_sim_destroy_reentrant_during_run(void) {
    sim_t *sim = NULL;
    sim_event_id_t event_id = 0;
    sim_time_t now = UINT64_MAX;
    sim_status_t destroy_status = SIM_ERR_INVALID_ARGUMENT;

    CHECK(sim_create(&sim) == SIM_OK);
    CHECK(
        sim_schedule_at(sim, 5, destroy_sim_callback, &destroy_status, NULL, &event_id) == SIM_OK
    );

    CHECK(sim_run(sim) == SIM_OK);
    CHECK(destroy_status == SIM_ERR_REENTRANT);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 5);
    CHECK(sim_destroy(sim) == SIM_OK);

    return true;
}

static bool test_sim_destroy_reentrant_during_run_until(void) {
    sim_t *sim = NULL;
    sim_event_id_t event_id = 0;
    sim_time_t now = UINT64_MAX;
    sim_status_t destroy_status = SIM_ERR_INVALID_ARGUMENT;

    CHECK(sim_create(&sim) == SIM_OK);
    CHECK(
        sim_schedule_at(sim, 5, destroy_sim_callback, &destroy_status, NULL, &event_id) == SIM_OK
    );

    CHECK(sim_run_until(sim, 10) == SIM_OK);
    CHECK(destroy_status == SIM_ERR_REENTRANT);
    CHECK(sim_now(sim, &now) == SIM_OK);
    CHECK(now == 10);
    CHECK(sim_destroy(sim) == SIM_OK);

    return true;
}

static bool test_sim_now_null_arguments(void) {
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

static const test_case_t tests[] = {
    TEST_CASE("sim_create_valid_output", test_sim_create_valid_output),
    TEST_CASE("sim_create_null_output", test_sim_create_null_output),
    TEST_CASE("sim_create_independent_instances", test_sim_create_independent_instances),
    TEST_CASE("sim_create_after_destroy", test_sim_create_after_destroy),
    TEST_CASE("sim_destroy_null", test_sim_destroy_null),
    TEST_CASE("sim_destroy_discards_pending_events", test_sim_destroy_discards_pending_events),
    TEST_CASE("sim_destroy_cleans_pending_events", test_sim_destroy_cleans_pending_events),
    TEST_CASE("sim_destroy_reentrant_during_run", test_sim_destroy_reentrant_during_run),
    TEST_CASE(
        "sim_destroy_reentrant_during_run_until", test_sim_destroy_reentrant_during_run_until
    ),
    TEST_CASE("sim_now_null_arguments", test_sim_now_null_arguments),
};

test_suite_t test_suite_lifecycle(void) {
    return (test_suite_t){.tests = tests, .count = sizeof(tests) / sizeof(tests[0])};
}
