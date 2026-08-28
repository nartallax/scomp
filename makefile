.PHONY: build
build:
	rm -rf ./build
	mkdir ./build
	clang -DNDEBUG -std=c23 -Wall -Wextra -Wpedantic -O2 -o ./build/output arithmetic_compression/index.c

.PHONY: test
test:
	rm -rf ./build
	mkdir ./build
	clang -std=c23 -Wall -Wextra -Wpedantic -O2 -o ./build/test test/index.c
	./build/test
