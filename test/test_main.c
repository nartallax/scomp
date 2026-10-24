#include "test_list.c"
#include "test_utils.c"
#include <stdio.h>

int run_test_suite(const test *tests, size_t tester_count) {
  int64_t failed_tests = 0;
  for (size_t i = 0; i < tester_count; i++) {
    test current_test = tests[i];
    setup_test_context_default();
    const char *test_result = current_test.tester();
    if (test_result != NULL) {
      printf("Test \"%s\" failed: %s\n", current_test.name, test_result);
      failed_tests++;
    }
    free_test_context();
  }

  if (failed_tests > 0) {
    printf("Failed tests: %zu/%zu\n", failed_tests, tester_count);
    return 1;
  } else {
    printf("All tests passed.\n");
    return 0;
  }
}

int main(int argc, const char **argv) {
  if (argc > 1 && argv[1][0] == 'p') {
    // perf suite
    return run_test_suite(perf_tests, sizeof perf_tests / sizeof perf_tests[0]);
  } else {
    // normal suite
    return run_test_suite(normal_tests, sizeof normal_tests / sizeof normal_tests[0]);
  }
}