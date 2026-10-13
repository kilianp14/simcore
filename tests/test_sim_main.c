#include "test.h"

#include <string.h>

test_suite_t test_suite_lifecycle(void);

int main(int argc, char **argv) {
    const test_suite_t suites[] = {test_suite_lifecycle()};
    const size_t suite_count = sizeof(suites) / sizeof(suites[0]);
    size_t run_count = 0;
    size_t passed_count = 0;
    size_t i;
    size_t j;

    if (argc > 2) {
        fprintf(stderr, "Usage: %s [test-name]\n", argv[0]);
        return 2;
    }

    for (i = 0; i < suite_count; ++i) {
        for (j = 0; j < suites[i].count; ++j) {
            const test_case_t *test = &suites[i].tests[j];

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
    }

    if (argc == 2 && run_count == 0) {
        fprintf(stderr, "Unknown test: %s\nAvailable tests:\n", argv[1]);
        for (i = 0; i < suite_count; ++i) {
            for (j = 0; j < suites[i].count; ++j) {
                fprintf(stderr, "  %s\n", suites[i].tests[j].name);
            }
        }
        return 2;
    }

    printf("%zu/%zu tests passed\n", passed_count, run_count);
    return passed_count == run_count ? 0 : 1;
}
