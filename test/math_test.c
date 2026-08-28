#include "./test_utils.c"
#include "../src/math.c"

const char* test_is_power_of_two(){
	TEST_ASSERT(is_power_of_two(0) == false, "Zero should not be power-of-two");
	TEST_ASSERT(is_power_of_two(1) == true, "One should be power-of-two");
	TEST_ASSERT(is_power_of_two(2) == true, "Two should be power-of-two");
	TEST_ASSERT(is_power_of_two(3) == false, "Three should not be power-of-two");
	TEST_ASSERT(is_power_of_two(5) == false, "Five should not be power-of-two");
	TEST_ASSERT(is_power_of_two(255) == false, "255 should not be power-of-two");
	TEST_ASSERT(is_power_of_two(256) == true, "256 should be power-of-two");
	TEST_ASSERT(is_power_of_two(-256) == false, "Negatives should not be power-of-two");
	return NULL;
}