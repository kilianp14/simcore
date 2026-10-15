#include "event_queue.h"
#include "sim/sim.h"
#include "test.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Test helpers */

/* Shared logging context for event callbacks and cleanup functions. */
static logging_context_t logging_context;

/* Creates a valid event record for queue tests. */
static event_t make_event(sim_event_id_t id, sim_time_t time) {
    return (event_t){
        .id = id,
        .time = time,
        .callback = logging_callback,
        .user_context = &logging_context,
        .user_context_destroy = logging_cleanup_callback,
    };
}

/* Verifies that all fields of two event records match. */
static bool check_event_equal(const event_t *actual, const event_t *expected) {
    CHECK(actual->id == expected->id);
    CHECK(actual->time == expected->time);
    CHECK(actual->callback == expected->callback);
    CHECK(actual->user_context == expected->user_context);
    CHECK(actual->user_context_destroy == expected->user_context_destroy);

    return true;
}

/* Verifies that events are popped in the expected ID order and that queue is empty afterward. */
static bool
check_pop_order(event_queue_t *queue, const sim_event_id_t *expected_ids, size_t expected_count) {
    CHECK(event_queue_size(queue) == expected_count);

    for (size_t i = 0; i < expected_count; ++i) {
        event_t actual = {0};

        CHECK(event_queue_pop(queue, &actual));
        CHECK(actual.id == expected_ids[i]);
    }

    CHECK(event_queue_is_empty(queue));
    CHECK(event_queue_size(queue) == 0);

    return true;
}

/* Creation and destruction */

/* Verifies that a newly created queue is empty and can be destroyed. */
static bool test_event_queue_create_destroy_empty(void) {
    event_queue_t *queue = event_queue_create();

    CHECK(queue != NULL);
    CHECK(event_queue_is_empty(queue));
    CHECK(event_queue_size(queue) == 0);

    event_queue_destroy(queue);
    return true;
}

/* Verifies that destroying a NULL queue is harmless. */
static bool test_event_queue_destroy_null(void) {
    event_queue_destroy(NULL);
    return true;
}

/* Insertion */

/* Verifies that inserting a valid event increases the queue size. */
static bool test_event_queue_insert_single(void) {
    event_queue_t *queue = event_queue_create();
    event_t event = make_event(1, 10);

    CHECK(queue != NULL);
    CHECK(event_queue_insert(queue, &event));
    CHECK(!event_queue_is_empty(queue));
    CHECK(event_queue_size(queue) == 1);

    CHECK(event_queue_pop(queue, &event));
    event_queue_destroy(queue);
    return true;
}

/* Peek */

/* Verifies that peek returns the inserted event without removing it. */
static bool test_event_queue_peek_single(void) {
    event_queue_t *queue = event_queue_create();
    const event_t expected = make_event(1, 10);
    event_t actual = {0};

    CHECK(queue != NULL);
    CHECK(event_queue_insert(queue, &expected));
    CHECK(event_queue_peek(queue, &actual));
    CHECK(check_event_equal(&actual, &expected));

    CHECK(!event_queue_is_empty(queue));
    CHECK(event_queue_size(queue) == 1);

    CHECK(event_queue_pop(queue, &actual));
    event_queue_destroy(queue);
    return true;
}

/* Verifies that peek reports an empty queue without modifying the output. */
static bool test_event_queue_peek_empty(void) {
    event_queue_t *queue = event_queue_create();
    event_t out = make_event(99, 123);
    const event_t expected = out;

    CHECK(queue != NULL);
    CHECK(!event_queue_peek(queue, &out));
    CHECK(event_queue_is_empty(queue));
    CHECK(event_queue_size(queue) == 0);
    CHECK(check_event_equal(&out, &expected));

    event_queue_destroy(queue);
    return true;
}

/* Pop */

/* Verifies that pop returns the inserted event and empties the queue. */
static bool test_event_queue_pop_single(void) {
    event_queue_t *queue = event_queue_create();
    const event_t expected = make_event(1, 10);
    event_t actual = {0};

    CHECK(queue != NULL);
    CHECK(event_queue_insert(queue, &expected));
    CHECK(event_queue_pop(queue, &actual));
    CHECK(check_event_equal(&actual, &expected));

    CHECK(event_queue_is_empty(queue));
    CHECK(event_queue_size(queue) == 0);

    /* No callback was called. */
    CHECK(logging_context.callback_count == 0);
    CHECK(logging_context.cleanup_count == 0);

    event_queue_destroy(queue);
    return true;
}

/* Verifies that pop reports an empty queue without modifying the output. */
static bool test_event_queue_pop_empty(void) {
    event_queue_t *queue = event_queue_create();

    event_t out = make_event(99, 123);
    const event_t expected = out;

    CHECK(queue != NULL);
    CHECK(!event_queue_pop(queue, &out));
    CHECK(event_queue_is_empty(queue));
    CHECK(event_queue_size(queue) == 0);
    CHECK(check_event_equal(&out, &expected));

    event_queue_destroy(queue);
    return true;
}

/* Ordering */

/* Verifies that events are popped in ascending time order. */
static bool test_event_queue_orders_by_time(void) {
    event_queue_t *queue = event_queue_create();

    const event_t first_inserted = make_event(1, 30);
    const event_t second_inserted = make_event(2, 20);
    const event_t third_inserted = make_event(3, 10);

    const sim_event_id_t expected_ids[] = {3, 2, 1};

    CHECK(event_queue_insert(queue, &first_inserted));
    CHECK(event_queue_insert(queue, &second_inserted));
    CHECK(event_queue_insert(queue, &third_inserted));
    CHECK(check_pop_order(queue, expected_ids, 3));

    event_queue_destroy(queue);
    return true;
}

/* Verifies that same-time events are popped in ascending event-ID order and not insertion order. */
static bool test_event_queue_orders_equal_times_by_id(void) {
    event_queue_t *queue = event_queue_create();

    const event_t first_inserted = make_event(4, 7);
    const event_t second_inserted = make_event(2, 7);
    const event_t third_inserted = make_event(1, 7);

    const sim_event_id_t expected_ids[] = {1, 2, 4};

    CHECK(event_queue_insert(queue, &first_inserted));
    CHECK(event_queue_insert(queue, &second_inserted));
    CHECK(event_queue_insert(queue, &third_inserted));
    CHECK(check_pop_order(queue, expected_ids, 3));

    event_queue_destroy(queue);
    return true;
}

/* Verifies that event time takes precedence over event ID. */
static bool test_event_queue_orders_time_before_id(void) {
    event_queue_t *queue = event_queue_create();

    const event_t later_event = make_event(1, 20);
    const event_t earlier_event = make_event(99, 10);

    const sim_event_id_t expected_ids[] = {99, 1};

    CHECK(event_queue_insert(queue, &later_event));
    CHECK(event_queue_insert(queue, &earlier_event));
    CHECK(check_pop_order(queue, expected_ids, 2));

    event_queue_destroy(queue);
    return true;
}

/* Verifies ordering across multiple times and ids, including equal-time. */
static bool test_event_queue_orders_mixed_times_and_ids(void) {
    event_queue_t *queue = event_queue_create();

    const event_t events[] = {
        make_event(40, 8),
        make_event(15, 3),
        make_event(7, 8),
        make_event(22, 3),
        make_event(50, 5),
    };

    const sim_event_id_t expected_ids[] = {15, 22, 50, 7, 40};

    for (size_t i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        CHECK(event_queue_insert(queue, &events[i]));
    }
    CHECK(check_pop_order(queue, expected_ids, 5));

    event_queue_destroy(queue);
    return true;
}

/* Verifies ordering at the boundaries of the time and event-ID ranges. */
static bool test_event_queue_orders_boundary_values(void) {
    event_queue_t *queue = event_queue_create();

    const event_t events[] = {
        make_event(UINT64_MAX, UINT64_MAX),
        make_event(2, 0),
        make_event(UINT64_MAX - 1, 0),
        make_event(1, UINT64_MAX),
    };

    const sim_event_id_t expected_ids[] = {2, UINT64_MAX - 1, 1, UINT64_MAX};

    for (size_t i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        CHECK(event_queue_insert(queue, &events[i]));
    }
    CHECK(check_pop_order(queue, expected_ids, 4));
    event_queue_destroy(queue);
    return true;
}

/* Removal */

/* Verifies missing-ID removal on a non-empty and an empty queue. */
static bool test_event_queue_remove_missing_id_and_empty(void) {
    event_queue_t *queue = event_queue_create();

    const event_t events[] = {
        make_event(1, 10),
        make_event(2, 20),
        make_event(3, 30),
    };

    CHECK(queue != NULL);

    for (size_t i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        CHECK(event_queue_insert(queue, &events[i]));
    }

    event_t out = make_event(99, 123);
    const event_t expected_out = out;

    CHECK(!event_queue_remove(queue, 999, &out));
    CHECK(check_event_equal(&out, &expected_out));
    CHECK(event_queue_size(queue) == 3);

    event_t actual = {0};
    CHECK(event_queue_peek(queue, &actual));
    CHECK(check_event_equal(&actual, &events[0]));

    const sim_event_id_t expected_ids[] = {1, 2, 3};
    CHECK(check_pop_order(queue, expected_ids, 3));

    out = make_event(98, 456);
    const event_t expected_empty_out = out;

    CHECK(!event_queue_remove(queue, 999, &out));
    CHECK(check_event_equal(&out, &expected_empty_out));
    CHECK(event_queue_is_empty(queue));
    CHECK(event_queue_size(queue) == 0);

    event_queue_destroy(queue);
    return true;
}

/* Verifies that removing the only event empties the queue. */
static bool test_event_queue_remove_only(void) {
    event_queue_t *queue = event_queue_create();

    const event_t expected = make_event(42, 17);

    CHECK(queue != NULL);
    CHECK(event_queue_insert(queue, &expected));
    CHECK(event_queue_size(queue) == 1);

    event_t removed = {0};
    CHECK(event_queue_remove(queue, expected.id, &removed));
    CHECK(check_event_equal(&removed, &expected));
    CHECK(event_queue_is_empty(queue));
    CHECK(event_queue_size(queue) == 0);

    /* No callback was called. */
    CHECK(logging_context.callback_count == 0);
    CHECK(logging_context.cleanup_count == 0);

    event_queue_destroy(queue);
    return true;
}

/* Verifies that removing the root returns the earliest event and preserves ordering. */
static bool test_event_queue_remove_root(void) {
    event_queue_t *queue = event_queue_create();

    const event_t events[] = {
        make_event(1, 2),
        make_event(2, 5),
        make_event(3, 8),
        make_event(4, 12),
        make_event(5, 15),
    };

    for (size_t i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        CHECK(event_queue_insert(queue, &events[i]));
    }

    event_t removed = {0};
    CHECK(event_queue_remove(queue, events[0].id, &removed));
    CHECK(check_event_equal(&removed, &events[0]));
    CHECK(event_queue_size(queue) == 4);

    event_t actual = {0};
    CHECK(event_queue_peek(queue, &actual));
    CHECK(check_event_equal(&actual, &events[1]));

    const sim_event_id_t expected_ids[] = {2, 3, 4, 5};
    CHECK(check_pop_order(queue, expected_ids, 4));

    event_queue_destroy(queue);
    return true;
}

/* Verifies that removing the last event preserves the remaining queue order. */
static bool test_event_queue_remove_last(void) {
    event_queue_t *queue = event_queue_create();

    const event_t events[] = {
        make_event(1, 10),
        make_event(2, 20),
        make_event(3, 30),
        make_event(4, 40),
        make_event(5, 50),
        make_event(6, 60),
        make_event(7, 70),
    };

    for (size_t i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        CHECK(event_queue_insert(queue, &events[i]));
    }

    event_t removed = {0};
    CHECK(event_queue_remove(queue, events[6].id, &removed));
    CHECK(check_event_equal(&removed, &events[6]));
    CHECK(event_queue_size(queue) == 6);

    const sim_event_id_t expected_ids[] = {1, 2, 3, 4, 5, 6};
    CHECK(check_pop_order(queue, expected_ids, 6));

    event_queue_destroy(queue);
    return true;
}

/* Verifies that removing a middle event preserves the remaining queue order. */
static bool test_event_queue_remove_middle(void) {
    event_queue_t *queue = event_queue_create();

    const event_t events[] = {
        make_event(1, 10),
        make_event(2, 20),
        make_event(3, 30),
        make_event(4, 40),
        make_event(5, 50),
        make_event(6, 60),
        make_event(7, 70),
    };

    for (size_t i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        CHECK(event_queue_insert(queue, &events[i]));
    }

    event_t removed = {0};
    CHECK(event_queue_remove(queue, events[3].id, &removed));
    CHECK(check_event_equal(&removed, &events[3]));
    CHECK(event_queue_size(queue) == 6);

    event_t actual = {0};
    CHECK(event_queue_peek(queue, &actual));
    CHECK(check_event_equal(&actual, &events[0]));

    const sim_event_id_t expected_ids[] = {1, 2, 3, 5, 6, 7};
    CHECK(check_pop_order(queue, expected_ids, 6));

    event_queue_destroy(queue);
    return true;
}

/* Verifies repeated removals maintain the queue integrity. */
static bool test_event_queue_repeated_removal(void) {
    event_queue_t *queue = event_queue_create();

    const event_t events[] = {
        make_event(1, 1),
        make_event(50, 50),
        make_event(10, 10),
        make_event(60, 60),
        make_event(70, 70),
        make_event(15, 15),
        make_event(20, 20),
    };

    for (size_t i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        CHECK(event_queue_insert(queue, &events[i]));
    }

    event_t removed = {0};

    CHECK(event_queue_remove(queue, 60, &removed));
    CHECK(check_event_equal(&removed, &events[3]));
    CHECK(event_queue_size(queue) == 6);

    event_t actual = {0};
    CHECK(event_queue_peek(queue, &actual));
    CHECK(check_event_equal(&actual, &events[0]));

    CHECK(event_queue_remove(queue, 20, &removed));
    CHECK(check_event_equal(&removed, &events[6]));
    CHECK(event_queue_size(queue) == 5);

    CHECK(event_queue_peek(queue, &actual));
    CHECK(check_event_equal(&actual, &events[0]));

    CHECK(event_queue_remove(queue, 50, &removed));
    CHECK(check_event_equal(&removed, &events[1]));
    CHECK(event_queue_size(queue) == 4);

    const sim_event_id_t expected_ids[] = {1, 10, 15, 70};
    CHECK(check_pop_order(queue, expected_ids, 4));

    event_queue_destroy(queue);
    return true;
}

/* Re-allocation */

/* Verifies that the queue handles many insertions/ removals (no issue in re-allocation). */
static bool test_event_queue_stress(void) {
    enum { EVENT_COUNT = 200 };

    event_queue_t *queue = event_queue_create();
    event_t events[EVENT_COUNT];

    for (size_t i = 0; i < EVENT_COUNT; ++i) {
        events[i] = make_event((sim_event_id_t)(i + 1), (sim_time_t)(i + 1));
        CHECK(event_queue_insert(queue, &events[i]));
    }

    for (size_t i = 0; i < EVENT_COUNT; ++i) {
        event_t actual = {0};
        CHECK(event_queue_pop(queue, &actual));
    }

    event_queue_destroy(queue);
    return true;
}

static const test_case_t tests[] = {
    /* Creation + Destruction */
    TEST_CASE("event_queue_create_destroy_empty", test_event_queue_create_destroy_empty),
    TEST_CASE("event_queue_destroy_null", test_event_queue_destroy_null),

    /* Insertion */
    TEST_CASE("event_queue_insert_single", test_event_queue_insert_single),

    /* Peek */
    TEST_CASE("event_queue_peek_single", test_event_queue_peek_single),
    TEST_CASE("event_queue_peek_empty", test_event_queue_peek_empty),

    /* Pop */
    TEST_CASE("event_queue_pop_single", test_event_queue_pop_single),
    TEST_CASE("event_queue_pop_empty", test_event_queue_pop_empty),

    /* Ordering */
    TEST_CASE("event_queue_orders_by_time", test_event_queue_orders_by_time),
    TEST_CASE("event_queue_orders_equal_times_by_id", test_event_queue_orders_equal_times_by_id),
    TEST_CASE("event_queue_orders_time_before_id", test_event_queue_orders_time_before_id),
    TEST_CASE(
        "event_queue_orders_mixed_times_and_ids", test_event_queue_orders_mixed_times_and_ids
    ),
    TEST_CASE("event_queue_orders_boundary_values", test_event_queue_orders_boundary_values),

    /* Removal */
    TEST_CASE(
        "event_queue_remove_missing_id_and_empty", test_event_queue_remove_missing_id_and_empty
    ),
    TEST_CASE("event_queue_remove_only", test_event_queue_remove_only),
    TEST_CASE("event_queue_remove_root", test_event_queue_remove_root),
    TEST_CASE("event_queue_remove_last", test_event_queue_remove_last),
    TEST_CASE("event_queue_remove_middle", test_event_queue_remove_middle),
    TEST_CASE("event_queue_repeated_removal", test_event_queue_repeated_removal),

    /* Re-allocation */
    TEST_CASE("event_queue_stress", test_event_queue_stress),

};

int main(int argc, char **argv) {
    const size_t test_count = sizeof(tests) / sizeof(tests[0]);
    size_t run_count = 0;
    size_t passed_count = 0;
    size_t i;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [test-name]\n", argv[0]);
        return 2;
    }

    for (i = 0; i < test_count; ++i) {
        const test_case_t *test = &tests[i];

        if (argc == 2 && strcmp(argv[1], test->name) != 0) {
            continue;
        }

        ++run_count;

        if (test->run()) {
            ++passed_count;
            printf("PASS: %s\n", test->name);
        } else {
            fprintf(stderr, "FAIL: %s\n", test->name);
        }
    }

    if (argc == 2 && run_count == 0) {
        fprintf(stderr, "Unknown test: %s\nAvailable tests:\n", argv[1]);

        for (i = 0; i < test_count; ++i) {
            fprintf(stderr, "  %s\n", tests[i].name);
        }

        return 2;
    }

    printf("%zu/%zu tests passed\n", passed_count, run_count);
    return passed_count == run_count ? 0 : 1;
}
