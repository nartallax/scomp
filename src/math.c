#pragma once

#include <stdbit.h>
#include <stdbool.h>
#include <stddef.h>

bool is_power_of_two(size_t n) {
  return n != 0 && (n & (n - 1)) == 0;
}
