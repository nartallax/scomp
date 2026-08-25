.PHONY: build
build:
	rm -rf ./build
	mkdir ./build
	clang -Wall -Wextra -O2 -o ./build/output arithmetic_compression/index.c

.PHONY: test
test:
	rm -rf ./build
	mkdir ./build
	clang -Wall -Wextra -O2 -o ./build/test test/index.c
	./build/test
