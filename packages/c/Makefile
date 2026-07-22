CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -Iinclude
SOURCES := $(wildcard src/*.c)
TESTS := tests/test_public_headers tests/test_pixa_blit tests/test_pixa_decode tests/test_pixa_dir

.PHONY: check clean

check: $(TESTS)
	./tests/test_public_headers
	./tests/test_pixa_blit
	./tests/test_pixa_decode
	./tests/test_pixa_dir

tests/test_public_headers: tests/test_public_headers.c
	$(CC) $(CFLAGS) $< -o $@

tests/test_pixa_blit: tests/test_pixa_blit.c $(SOURCES)
	$(CC) $(CFLAGS) $^ -o $@

tests/test_pixa_decode: tests/test_pixa_decode.c $(SOURCES)
	$(CC) $(CFLAGS) $^ -o $@

tests/test_pixa_dir: tests/test_pixa_dir.c $(SOURCES)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(TESTS)
