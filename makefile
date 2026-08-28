MAKEFLAGS += --no-print-directory

CC=clang -std=c23 -Wall -Wextra -Wpedantic

.PHONY: clear
clear:
	@rm -rf ./build
	@mkdir ./build

.PHONY: build_test
build_test:
	@make clear
	@$(CC) -O2 -g -o ./build/test test/test_index.c

.PHONY: build_test_profiled
build_test_profiled:
	@make clear
	@# -DNDEBUG here because I don't want to calculate code coverage on asserts
	@$(CC) -DNDEBUG -O2 -fprofile-instr-generate -fcoverage-mapping -g -o ./build/test test/test_index.c

.PHONY: run_test_profiled
run_test_profiled: 
	@make build_test_profiled
	@LLVM_PROFILE_FILE="./build/llvm_profile.profraw" ./build/test

.PHONY: build
build:
	@make clear
	@$(CC) -DNDEBUG -O2 -o ./build/output arithmetic_compression/index.c

.PHONY: run_test
run_test:
	@make build_test
	@./build/test

.PHONY: coverage
coverage:
	@make run_test_profiled
	@llvm-profdata merge -sparse ./build/llvm_profile.profraw -o ./build/llvm_profile.profdata
	@llvm-cov report ./build/test -instr-profile=./build/llvm_profile.profdata ./src/
	@llvm-cov show ./build/test -instr-profile=./build/llvm_profile.profdata -format=html -show-line-counts-or-regions -show-branches=count -show-expansions -output-dir=./build/coverage ./src/
	@echo Full report available at file://`realpath ./build/coverage/index.html`

.PHONY: check-coverage
check-coverage: 
	@make coverage
	@llvm-cov report ./build/test -instr-profile=./build/llvm_profile.profdata ./src/ | grep -E "^TOTAL" | grep -E "[0-9]+\.[0-9]+%$$" -o | grep -E "^[0-9]+" -o > ./build/test_coverage_percent_number.txt
	@TOTAL_COVERAGE=$$(cat ./build/test_coverage_percent_number.txt); \
	EXPECTED_COVERAGE=99;  \
	if [ "$$TOTAL_COVERAGE" -lt "$$EXPECTED_COVERAGE" ]; then \
		echo "Total code coverage is too low: $$TOTAL_COVERAGE% < $$EXPECTED_COVERAGE%"; \
		exit 1; \
	else \
		echo "Coverage $$TOTAL_COVERAGE% is above minimum limit $$EXPECTED_COVERAGE%"; \
	fi

.PHONY: valgrind
valgrind:
	@make build_test
	@#I'm not entirely sure why setting file descriptor limit is required, but it only runs for me like that.
	@#if it causes problems on your machine - consider removing ulimit call
	@ulimit -n 65536 && valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./build/test

.PHONY: check-format
check-format:
	@clang-format --dry-run --Werror --style=file src/* test/* && echo "All files are formatted correctly."

# this one test everything there is to test
.PHONY: test
test: 
	@make run_test
	@make check-coverage
	@make valgrind
	@make check-format
	@echo "All checks are successful."