HOST_CC ?= gcc

BUILD_DIR := build
TEST_BIN := $(BUILD_DIR)/test_sanity
TEST_CFLAGS := -std=c11 -O1 -g3 -Wall -Wextra -Werror -Wconversion \
	-fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: all clean test

all: test

test: $(TEST_BIN)
	ASAN_OPTIONS=detect_leaks=0 ./$(TEST_BIN)

$(TEST_BIN): test_sanity.c | $(BUILD_DIR)
	$(HOST_CC) $(TEST_CFLAGS) $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)
