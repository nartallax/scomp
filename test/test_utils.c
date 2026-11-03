#pragma once
#include "../src/context.c"
#include "../src/data_structures/buffer.c"
#include "../src/data_structures/writer.c"
#include <dirent.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define TEST_ASSERT(expr)                                                                                                                                                                              \
  do {                                                                                                                                                                                                 \
    if (!(expr)) {                                                                                                                                                                                     \
      return #expr;                                                                                                                                                                                    \
    }                                                                                                                                                                                                  \
  } while (0)

int test_allocations_before_failure = 0;
context *test_context = NULL;

void _test_reset_allocators() {
  context_set_allocators(test_context, malloc, calloc, realloc, free);
}

void *test_malloc_with_counter(memsize size) {
  // printf("malloc(%zu), allocations left: %i\n", size, test_allocations_before_failure);
  if (test_allocations_before_failure < 1) {
    _test_reset_allocators();
    return NULL;
  }
  test_allocations_before_failure--;
  return malloc(size);
}

void *test_calloc_with_counter(memsize count, memsize size) {
  // printf("calloc(%zu, %zu), allocations left: %i\n", count, size, test_allocations_before_failure);
  if (test_allocations_before_failure < 1) {
    _test_reset_allocators();
    return NULL;
  }
  test_allocations_before_failure--;
  return calloc(count, size);
}

void *test_realloc_with_counter(void *base, memsize size) {
  // printf("realloc(%zu), allocations left: %i\n", size, test_allocations_before_failure);
  if (test_allocations_before_failure < 1) {
    _test_reset_allocators();
    return NULL;
  }
  test_allocations_before_failure--;
  return realloc(base, size);
}

void free_test_context() {
  if (test_context) {
    free(test_context);
    test_context = NULL;
  }
}

void update_test_context_for_alloc_failure(int allocations_before_failure_count) {
  malloc_fn mlc = malloc;
  calloc_fn clc = calloc;
  realloc_fn rlc = realloc;

  if (allocations_before_failure_count >= 0) {
    mlc = test_malloc_with_counter;
    clc = test_calloc_with_counter;
    rlc = test_realloc_with_counter;
    // +1 for the context allocation itself
    test_allocations_before_failure = allocations_before_failure_count;
  }

  context_set_allocators(test_context, mlc, clc, rlc, free);
}

void setup_test_context(int allocations_before_failure_count) {
  free_test_context();
  test_context = malloc(sizeof(context));
  bool context_init_success = context_init(test_context, malloc, calloc, realloc, free);
  if (!context_init_success) {
    printf("Failed to init test context. This is not supposed to ever happen.\n");
    abort();
  }
  update_test_context_for_alloc_failure(allocations_before_failure_count);
}

void setup_test_context_default() {
  setup_test_context(-1);
}

char *string_repeat(const char *base, int times) {
  memsize length = strlen(base);
  memsize result_length = (times * length) + 1;
  char *result = malloc(sizeof(char) * result_length);
  for (int i = 0; i < times; i++) {
    for (memsize j = 0; j < length; j++) {
      result[(i * length) + j] = base[j];
    }
  }
  result[result_length - 1] = 0;
  return result;
}

char *string_repeat_sprintf(const char *format, const char *base, int times) {
  char *rep = string_repeat(base, times);
  memsize total_len = (strlen(format) * 10) + strlen(rep);
  char *result = malloc(sizeof(char) * total_len);
  snprintf(result, total_len, format, rep);
  free(rep);
  return result;
}

bool foreach_file_in_directory(const char *directory_path, void (*do_with_file)(const char *full_filepath)) {
  DIR *directory = opendir(directory_path);

  if (directory == NULL) {
    return false;
  }

  struct dirent *entry;
  char file_path[4096];
  bool is_full_success = true;

  while ((entry = readdir(directory)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    int file_path_length = snprintf(file_path, sizeof(file_path), "%s/%s", directory_path, entry->d_name);
    if (file_path_length < 0 || (memsize)file_path_length >= sizeof(file_path)) {
      is_full_success = false;
      continue;
    }

    struct stat file_info;
    if (stat(file_path, &file_info) != 0) {
      is_full_success = false;
      continue;
    }

    // only go over files, not directories or something else
    if (!S_ISREG(file_info.st_mode)) {
      is_full_success = false;
      continue;
    }

    do_with_file(file_path);
  }

  closedir(directory);
  return is_full_success;
}

const char *get_filename(const char *path) {
  const char *last_slash = strrchr(path, '/');

  if (last_slash != NULL) {
    return last_slash + 1;
  }

  // windows?
  const char *last_bslash = strrchr(path, '\\');
  if (last_bslash != NULL) {
    return last_bslash + 1;
  }

  return NULL;
}

bool starts_with(const char *string, const char *prefix) {
  int64 prefix_length = strlen(prefix);

  return strncmp(string, prefix, prefix_length) == 0;
}

bool ends_with(const char *string, const char *suffix) {
  int64 string_length = strlen(string);
  int64 suffix_length = strlen(suffix);

  if (suffix_length > string_length) {
    return false;
  }

  return strcmp(string + string_length - suffix_length, suffix) == 0;
}

#define TIME_STRUCT struct timespec

TIME_STRUCT get_current_time() {
  TIME_STRUCT result;
  timespec_get(&result, TIME_UTC);
  return result;
}

double get_time_diff(TIME_STRUCT start, TIME_STRUCT end) {
  double seconds = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
  return seconds;
}

// trailing zero excluded
buffer malloc_string_buffer(const char *base) {
  int64 length = strlen(base);
  byte *data = malloc(length);
  memcpy(data, base, length);
  return buffer_make(data, length, length);
}

buffer test_read_file_to_buffer(const char *path) {
  FILE *src_file = fopen(path, "r");
  int64 bytes_read;
  char read_buffer[4096];
  writer w;
  if (!writer_init(&w, test_context, 4096, 4)) {
    return EMPTY_BUFFER;
  }
  while ((bytes_read = fread(read_buffer, 1, sizeof(read_buffer), src_file)) > 0) {
    for (int64 i = 0; i < bytes_read; i++) {
      writer_write_byte(&w, read_buffer[i]);
    }
  }
  buffer result = writer_consume_all_buffers(&w).buffer;
  writer_deinit(&w, test_context);
  return result;
}

buffer test_get_n_bytes_around_index(buffer src, int64 index, int64 range) {
  int64 min = index - range;
  if (min < 0) {
    min = 0;
  }
  int64 max = index + range;
  if (max > src.length) {
    max = src.length;
  }

  return buffer_make(src.data + min, max - min, max - min);
}

bool test_compare_buffers(const char *name, buffer expected, buffer got) {
  FILE *out_file = fopen("/tmp/out.json", "w");
  TEST_ASSERT((int64)fwrite(got.data, 1, got.length, out_file) == got.length);
  fclose(out_file);

  int64 min_length = expected.length < got.length ? expected.length : got.length;
  for (int64 i = 0; i < min_length; i++) {
    if (expected.data[i] != got.data[i]) {
      printf("%s: at %zu, expected %c (%i), but got %c (%i)\n", name, i, expected.data[i], expected.data[i], got.data[i], got.data[i]);
      buffer exp_slice = test_get_n_bytes_around_index(expected, i, 50);
      buffer got_slice = test_get_n_bytes_around_index(got, i, 50);
      printf("expected:   %.*s\n", (int)exp_slice.length, exp_slice.data);
      printf("got:        %.*s\n", (int)got_slice.length, got_slice.data);
      return false;
    }
  }

  // checking length before content because I want to see the discrepancies
  if (expected.length != got.length) {
    printf("%s: expected to be %zi bytes long, but got %zi bytes\n", name, expected.length, got.length);
    return false;
  }

  return true;
}

bool test_compare_buffer_to_file(buffer b, const char *path) {
  buffer expected = test_read_file_to_buffer(path);
  bool result = test_compare_buffers(path, expected, b);
  buffer_free(expected, test_context);
  return result;
}