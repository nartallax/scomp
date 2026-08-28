#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdbit.h>

bool is_power_of_two(size_t n) {
    return n != 0 && (n & (n - 1)) == 0;
}

int size_exponent(size_t n){
    return stdc_bit_width(n) - 1;
}