#ifndef TEST_H
#define TEST_H

#include "sim/sim.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define NUM2STR(x) STR(x)
#define STR(x) #x

#define TEST_CASE(name, fn) {name, fn}
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fputs(__FILE__ ":" NUM2STR(__LINE__) " in ", stderr);                                  \
            fputs(__func__, stderr);                                                               \
            fputs(": check failed: " #condition "\n", stderr);                                     \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef bool (*test_fn)(void);

typedef struct test_case {
    const char *name;
    test_fn run;
} test_case_t;

typedef struct test_suite {
    const char *name;
    const test_case_t *tests;
    size_t count;
} test_suite_t;

static inline int test_run(const test_suite_t *suite, int argc, char **argv) {
    size_t run_count = 0;
    size_t passed_count = 0;
    size_t i;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [test-name]\n", argv[0]);
        return 2;
    }

    printf("=== Suite: %s ===\n", suite->name ? suite->name : "Unnamed");

    for (i = 0; i < suite->count; ++i) {
        const test_case_t *test = &suite->tests[i];

        if (argc == 2 && strcmp(argv[1], test->name) != 0) {
            continue;
        }

        ++run_count;
        if (test->run()) {
            ++passed_count;
            printf("  PASS: %s\n", test->name);
        } else {
            fprintf(stderr, "  FAIL: %s\n", test->name);
        }
    }

    if (argc == 2 && run_count == 0) {
        fprintf(stderr, "Unknown test: %s\nAvailable tests:\n", argv[1]);
        for (i = 0; i < suite->count; ++i) {
            fprintf(stderr, "  %s\n", suite->tests[i].name);
        }
        return 2;
    }

    printf(
        "%zu/%zu tests passed in %s\n\n",
        passed_count,
        run_count,
        suite->name ? suite->name : "suite"
    );
    return passed_count == run_count ? 0 : 1;
}

/* Context, callback and cleanup helpers that allow checking execution count. */
typedef struct {
    sim_event_id_t ids[8];
    sim_time_t times[8];
    size_t callback_count;
    size_t cleanup_count;
} logging_context_t;

static inline sim_status_t logging_callback(
    sim_t *sim, sim_event_id_t event_id, sim_time_t now, void *user_context
) {
    logging_context_t *log = user_context;
    (void)sim;

    log->ids[log->callback_count] = event_id;
    log->times[log->callback_count] = now;
    log->callback_count++;

    return SIM_OK;
}

static inline void logging_cleanup_callback(void *user_context) {
    logging_context_t *log = user_context;
    log->cleanup_count++;
}

#endif
