#pragma once

#include <stdbit.h>
#include <stdbool.h>
#include <stddef.h>

// TODO: do we need it?
bool is_power_of_two(size_t n) {
  return n != 0 && (n & (n - 1)) == 0;
}
