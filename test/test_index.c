#include <stdio.h>
#include "./writer_test.c"
#include "./math_test.c"

typedef const char *(*tester)(void);



int main(){
	tester testers[] = {
		test_writer_bytes,
		test_writer_bits,
		test_writer_wrong_init,
		test_is_power_of_two
		// tests go here
	};


	size_t tester_count = sizeof testers / sizeof testers[0];
	size_t failed_tests = 0;
	for(size_t i = 0; i < tester_count; i++){
		tester tester = testers[i];
		const char* test_result = tester();
		if(test_result != NULL){
			printf("Test failed: %s\n", test_result);
			failed_tests++;
		}
	}

	if(failed_tests > 0){
		printf("Failed tests: %zu/%zu\n", failed_tests, tester_count);
		return 1;
	} else {
		printf("All tests passed.\n");
		return 0;
	}
}