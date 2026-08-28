.PHONY: build
build:
	@rm -rf ./build
	@mkdir ./build
	@clang -DNDEBUG -std=c23 -Wall -Wextra -Wpedantic -O2 -o ./build/output arithmetic_compression/index.c

.PHONY: test
test:
	@rm -rf ./build
	@mkdir ./build
	@clang -std=c23 -Wall -Wextra -Wpedantic -O2 -o ./build/test test/test_index.c
	@./build/test

.PHONY: coverage
coverage:
	@rm -rf ./build
	@mkdir ./build
	@# -DNDEBUG here because I don't want to calculate code coverage on asserts
	@clang -DNDEBUG -std=c23 -Wall -Wextra -Wpedantic -O2 -fprofile-instr-generate -fcoverage-mapping -g -o ./build/test test/test_index.c
	@LLVM_PROFILE_FILE="./build/llvm_profile.profraw" ./build/test
	@llvm-profdata merge -sparse ./build/llvm_profile.profraw -o ./build/llvm_profile.profdata
	@llvm-cov report ./build/test -instr-profile=./build/llvm_profile.profdata ./src/
	@llvm-cov show ./build/test -instr-profile=./build/llvm_profile.profdata -format=html -show-line-counts-or-regions -show-branches=count -show-expansions -output-dir=./build/coverage ./src/
	@echo Full report available as html at file://`realpath ./build/coverage/index.html`