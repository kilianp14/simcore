#ifndef TEST_H
#define TEST_H

#include "sim/sim.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_CASE(name, fn) {name, fn}
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef bool (*test_fn)(void);

typedef struct test_case {
    const char *name;
    test_fn run;
} test_case_t;

typedef struct test_suite {
    const test_case_t *tests;
    size_t count;
} test_suite_t;

static inline sim_status_t
simple_count_callback(sim_t *sim, sim_event_id_t event_id, sim_time_t now, void *user_context) {
    int *count = user_context;
    (void)sim;
    (void)event_id;
    (void)now;
    ++*count;
    return SIM_OK;
}

typedef struct {
    int value;
    int *total;
    int *destroy_count;
} owned_context_t;

static sim_status_t
owned_callback(sim_t *sim, sim_event_id_t event_id, sim_time_t now, void *user_context) {
    owned_context_t *ctx = user_context;
    (void)sim;
    (void)event_id;
    (void)now;

    *ctx->total += ctx->value;
    return SIM_OK;
}

static void owned_context_destroy(void *user_context) {
    owned_context_t *ctx = user_context;

    ++(*ctx->destroy_count);
    free(ctx);
}

static sim_status_t
destroy_sim_callback(sim_t *sim, sim_event_id_t event_id, sim_time_t now, void *user_context) {
    sim_status_t *status = user_context;
    (void)event_id;
    (void)now;

    *status = sim_destroy(sim);
    return SIM_OK;
}

#endif
