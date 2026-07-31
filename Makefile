CC ?= cc
CPPFLAGS ?= -Iinclude
CFLAGS ?= -O3 -std=c99
WARNFLAGS = -Wall -Wextra -Wpedantic -Wshadow -Wundef

BUILD_DIR = build
BIN_DIR = bin
REF_OBJ = $(BUILD_DIR)/maskaglia_ref.o
RANDOMCOINS_OBJ = $(BUILD_DIR)/rng_shake.o
TEST_BIN = $(BIN_DIR)/test_distribution
BENCH_BIN = $(BIN_DIR)/bench_sampler

.PHONY: all test bench clean

all: $(TEST_BIN) $(BENCH_BIN)

test: $(TEST_BIN)
	./$(TEST_BIN)

bench: $(BENCH_BIN)
	./$(BENCH_BIN)

$(BUILD_DIR) $(BIN_DIR):
	mkdir -p $@

$(REF_OBJ): src/maskaglia_ref.c include/maskaglia.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ $<

$(RANDOMCOINS_OBJ): src/rng_shake.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c -o $@ $<

$(TEST_BIN): tests/test_distribution.c $(REF_OBJ) $(RANDOMCOINS_OBJ) | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ \
		tests/test_distribution.c $(REF_OBJ) $(RANDOMCOINS_OBJ)

$(BENCH_BIN): bench/bench_sampler.c $(REF_OBJ) $(RANDOMCOINS_OBJ) | $(BIN_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -o $@ \
		bench/bench_sampler.c $(REF_OBJ) $(RANDOMCOINS_OBJ)

clean:
	rm -f $(REF_OBJ) $(RANDOMCOINS_OBJ) $(TEST_BIN) $(BENCH_BIN)
