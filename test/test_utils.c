#pragma once
#include "../src/context.c"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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

void *test_malloc_with_counter(size_t size) {
  // printf("malloc(%zu), allocations left: %i\n", size, test_allocations_before_failure);
  if (test_allocations_before_failure < 1) {
    _test_reset_allocators();
    return NULL;
  }
  test_allocations_before_failure--;
  return malloc(size);
}

void *test_calloc_with_counter(size_t count, size_t size) {
  // printf("calloc(%zu, %zu), allocations left: %i\n", count, size, test_allocations_before_failure);
  if (test_allocations_before_failure < 1) {
    _test_reset_allocators();
    return NULL;
  }
  test_allocations_before_failure--;
  return calloc(count, size);
}

void *test_realloc_with_counter(void *base, size_t size) {
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
    context_delete(test_context);
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
  test_context = context_new(malloc, calloc, realloc, free);
  update_test_context_for_alloc_failure(allocations_before_failure_count);
}

void setup_test_context_default() {
  setup_test_context(-1);
}

char *string_repeat(const char *base, size_t length, int times) {
  size_t result_length = (times * length) + 1;
  char *result = malloc(sizeof(char) * result_length);
  for (int i = 0; i < times; i++) {
    for (size_t j = 0; j < length; j++) {
      result[(i * length) + j] = base[j];
    }
  }
  result[result_length - 1] = 0;
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
    if (file_path_length < 0 || (size_t)file_path_length >= sizeof(file_path)) {
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

    /*
      // char buffer[4096];
        FILE *file = fopen(file_path, "r");
        if (file == NULL) {
          is_full_success = false;
          continue;
        }

        size_t bytes_read;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
          fwrite(buffer, 1, bytes_read, stdout);
        }

        if (ferror(file)) {
          is_full_success = false;
        }

        fclose(file);
        */
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
  size_t prefix_length = strlen(prefix);

  return strncmp(string, prefix, prefix_length) == 0;
}

bool ends_with(const char *string, const char *suffix) {
  size_t string_length = strlen(string);
  size_t suffix_length = strlen(suffix);

  if (suffix_length > string_length) {
    return false;
  }

  return strcmp(string + string_length - suffix_length, suffix) == 0;
}