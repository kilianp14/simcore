#ifndef TEST_H
#define TEST_H

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

#endif
