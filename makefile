CC=clang -std=c23 -Wall -Wextra -Wpedantic
BUILD_TEST=

.PHONY: clear
clear:
	@rm -rf ./build
	@mkdir ./build

.PHONY: build_test
build_test: clear
	@$(CC) -O2 -g -o ./build/test test/test_index.c

.PHONY: build
build: clear
	@$(CC) -DNDEBUG -O2 -o ./build/output arithmetic_compression/index.c

.PHONY: test
test: build_test
	@./build/test

.PHONY: coverage
coverage: clear
	@# -DNDEBUG here because I don't want to calculate code coverage on asserts
	@$(CC) -DNDEBUG -O2 -fprofile-instr-generate -fcoverage-mapping -g -o ./build/test test/test_index.c
	@LLVM_PROFILE_FILE="./build/llvm_profile.profraw" ./build/test
	@llvm-profdata merge -sparse ./build/llvm_profile.profraw -o ./build/llvm_profile.profdata
	@llvm-cov report ./build/test -instr-profile=./build/llvm_profile.profdata ./src/
	@llvm-cov show ./build/test -instr-profile=./build/llvm_profile.profdata -format=html -show-line-counts-or-regions -show-branches=count -show-expansions -output-dir=./build/coverage ./src/
	@echo Full report available at file://`realpath ./build/coverage/index.html`

.PHONY: valgrind
valgrind: build_test
	@#I'm not entirely sure why setting file descriptor limit is required, but it only runs for me like that.
	@#if it causes problems on your machine - consider removing ulimit call
	@ulimit -n 65536 && valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 ./build/test