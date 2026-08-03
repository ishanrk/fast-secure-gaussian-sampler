CXX ?= c++
CPPFLAGS ?= -Iinclude
CXXFLAGS ?= -O3 -std=c++17
WARNFLAGS = -Wall -Wextra -Wpedantic -Wshadow -Wundef -Wold-style-cast

BUILD_DIR = build
BIN_DIR = bin
REF_OBJ = $(BUILD_DIR)/maskaglia_ref.o
CDT_OBJ = $(BUILD_DIR)/cdt_ref.o
KNUTH_YAO_OBJ = $(BUILD_DIR)/knuth_yao_ref.o
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

$(REF_OBJ): src/maskaglia_ref.cpp include/maskaglia.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARNFLAGS) -c -o $@ $<

$(CDT_OBJ): src/cdt_ref.cpp include/maskaglia.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARNFLAGS) -c -o $@ $<

$(KNUTH_YAO_OBJ): src/knuth_yao_ref.cpp include/maskaglia.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARNFLAGS) -c -o $@ $<

$(RANDOMCOINS_OBJ): src/rng_shake.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARNFLAGS) -c -o $@ $<

$(TEST_BIN): tests/test_distribution.cpp $(REF_OBJ) $(CDT_OBJ) $(KNUTH_YAO_OBJ) $(RANDOMCOINS_OBJ) | $(BIN_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARNFLAGS) -o $@ \
		tests/test_distribution.cpp $(REF_OBJ) $(CDT_OBJ) $(KNUTH_YAO_OBJ) $(RANDOMCOINS_OBJ)

$(BENCH_BIN): bench/bench_sampler.cpp $(REF_OBJ) $(CDT_OBJ) $(KNUTH_YAO_OBJ) $(RANDOMCOINS_OBJ) | $(BIN_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(WARNFLAGS) -o $@ \
		bench/bench_sampler.cpp $(REF_OBJ) $(CDT_OBJ) $(KNUTH_YAO_OBJ) $(RANDOMCOINS_OBJ)

clean:
	rm -f $(REF_OBJ) $(CDT_OBJ) $(KNUTH_YAO_OBJ) $(RANDOMCOINS_OBJ) $(TEST_BIN) $(BENCH_BIN)
