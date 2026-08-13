CC ?= cc
AR ?= ar
OUT ?= build

CPPFLAGS ?=
PROJECT_CPPFLAGS = -Iinclude
CFLAGS ?= -O3
WARNFLAGS = -std=c99 -Wall -Wextra -Wpedantic -Wshadow -Wconversion \
	-Wstrict-prototypes -Wmissing-prototypes
LDLIBS = -lm

SOURCES = src/core/rng.c src/core/params.c src/ref/sampler.c \
	src/portable/pack.c
OBJECTS = $(SOURCES:%.c=$(OUT)/%.o)
DEPENDENCIES = $(OBJECTS:.o=.d)
LIBRARY = $(OUT)/libgaussian_sampler.a
TESTS = $(OUT)/test_rng $(OUT)/test_ref $(OUT)/test_pack \
	$(OUT)/test_stats
BENCHMARKS = $(OUT)/bench_ref $(OUT)/bench_pack

.PHONY: all test test-fast test-guard bench sanitize clean

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^

$(OUT)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(PROJECT_CPPFLAGS) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) \
		-MMD -MP -c $< -o $@

$(OUT)/test_%: tests/test_%.c $(LIBRARY) tests/test_util.h
	$(CC) $(PROJECT_CPPFLAGS) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) \
		-MMD -MP -MF $@.d -MT $@ $< $(LIBRARY) $(LDLIBS) -o $@

test-fast: $(OUT)/test_rng $(OUT)/test_ref $(OUT)/test_pack
	@$(OUT)/test_rng
	@$(OUT)/test_ref
	@$(OUT)/test_pack

test-guard:
	@mkdir -p $(OUT)
	@$(CC) $(PROJECT_CPPFLAGS) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) -c \
		tests/test_guard.c -o $(OUT)/test_guard_control.o
	@if $(CC) $(PROJECT_CPPFLAGS) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) \
		-DMG_ENABLE_MASKED -c tests/test_guard.c \
		-o $(OUT)/test_guard.o >$(OUT)/test_guard.log 2>&1; then \
		echo "test_guard: compile unexpectedly passed"; exit 1; \
	elif grep -q "masked maskaglia backend not implemented yet" \
		$(OUT)/test_guard.log; then \
		echo "test_guard: ok"; \
	else \
		cat $(OUT)/test_guard.log; \
		echo "test_guard: compile failed for an unexpected reason"; exit 1; \
	fi

test: $(TESTS) test-guard
	@$(OUT)/test_rng
	@$(OUT)/test_ref
	@$(OUT)/test_pack
	@$(OUT)/test_stats

$(OUT)/bench_%: bench/bench_%.c $(LIBRARY)
	$(CC) $(PROJECT_CPPFLAGS) $(CPPFLAGS) $(CFLAGS) $(WARNFLAGS) \
		-MMD -MP -MF $@.d -MT $@ $< $(LIBRARY) $(LDLIBS) -o $@

bench: $(BENCHMARKS)
	@$(OUT)/bench_ref
	@$(OUT)/bench_pack

sanitize:
	rm -rf $(OUT)/san
	ASAN_OPTIONS=detect_leaks=0 $(MAKE) OUT=$(OUT)/san CFLAGS="-O1 -g \
		-fsanitize=address,undefined \
		-fno-omit-frame-pointer" test

clean:
	rm -rf $(OUT)

-include $(DEPENDENCIES) $(TESTS:%=%.d) $(BENCHMARKS:%=%.d)
