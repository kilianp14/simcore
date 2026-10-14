#ifndef TEST_H
#define TEST_H

#include "sim/sim.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

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

typedef struct {
    sim_event_id_t ids[8];
    sim_time_t times[8];
    size_t callback_count;
    size_t cleanup_count;
} logging_context_t;

static inline sim_status_t
logging_callback(sim_t *sim, sim_event_id_t event_id, sim_time_t now, void *user_context) {
    logging_context_t *log = user_context;
    (void)sim;

    log->ids[log->callback_count] = event_id;
    log->times[log->callback_count] = now;
    ++log->callback_count;

    return SIM_OK;
}

static inline void logging_cleanup_callback(void *user_context) {
    logging_context_t *log = user_context;
    ++log->cleanup_count;
}

#endif
