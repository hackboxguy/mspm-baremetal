HOST_CC ?= gcc

BUILD_DIR := build
TEST_BIN := $(BUILD_DIR)/test_ringbuf
TEST_SRCS := test_ringbuf.c ../lib/lib_boot.c ../lib/lib_buildinfo.c ../lib/lib_crash.c \
	../lib/lib_debug.c ../lib/lib_regmap.c ../lib/lib_ringbuf.c
TEST_CFLAGS := -std=c11 -O1 -g3 -Wall -Wextra -Werror -Wconversion -Wshadow -Wundef \
	-fsanitize=address,undefined -fno-omit-frame-pointer -I../lib

.PHONY: all clean test

all: test

test: $(TEST_BIN)
	ASAN_OPTIONS=detect_leaks=0 ./$(TEST_BIN)

$(TEST_BIN): $(TEST_SRCS) | $(BUILD_DIR)
	$(HOST_CC) $(TEST_CFLAGS) $(TEST_SRCS) -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)
