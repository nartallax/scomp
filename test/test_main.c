#include "test_list.c"
#include "test_utils.c"
#include <stdio.h>

int main() {
  size_t tester_count = sizeof normal_tests / sizeof normal_tests[0];
  size_t failed_tests = 0;
  for (size_t i = 0; i < tester_count; i++) {
    test current_test = normal_tests[i];
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