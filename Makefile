CC ?= cc
AR ?= ar
OUT ?= build
PREFIX ?= /usr/local

CPPFLAGS ?=
CPPFLAGS += -Iinclude -Isrc
ADAPTER_CPPFLAGS := -Iadapters/hawk
CFLAGS ?= -O2
WARNFLAGS := -Wall -Wextra -Wpedantic -Werror -Wmissing-prototypes
WARNFLAGS += -Wshadow -Wpointer-arith -Wredundant-decls -Wconversion
WARNFLAGS += -Wsign-conversion
ALL_CFLAGS := $(CFLAGS) -std=c99 $(WARNFLAGS)
LDFLAGS ?=
LDLIBS ?=

LIB := $(OUT)/libpqsamp.a
HAWK_LIB := $(OUT)/libpqsamp_hawk.a
SOURCES := src/rng.c src/params.c src/bitslice.c src/masked.c
SOURCES += src/sampler.c src/reference.c src/generate.c
HAWK_SOURCES := adapters/hawk/pqsamp_hawk.c
OBJECTS := $(SOURCES:%.c=$(OUT)/%.o)
HAWK_OBJECTS := $(HAWK_SOURCES:%.c=$(OUT)/%.o)
DEPS := $(OBJECTS:.o=.d) $(HAWK_OBJECTS:.o=.d)

TEST_NAMES := test_core test_sampler test_adapter
TEST_BINS := $(TEST_NAMES:%=$(OUT)/tests/%)
EXAMPLE_NAMES := sample hawk_adapter
EXAMPLE_BINS := $(EXAMPLE_NAMES:%=$(OUT)/examples/%)

.PHONY: all test test-fast examples demo bench oracle sanitize check-clang
.PHONY: cross-m4 cross-rv32 assembly stack valgrind fuzz cbmc clean
.PHONY: install

all: $(LIB) $(HAWK_LIB)

$(LIB): $(OBJECTS)
	@mkdir -p $(@D)
	$(RM) $@
	$(AR) rcs $@ $^

$(HAWK_LIB): $(HAWK_OBJECTS)
	@mkdir -p $(@D)
	$(RM) $@
	$(AR) rcs $@ $^

$(OUT)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) -MMD -MP -c $< -o $@

$(OUT)/tests/%: tests/%.c $(LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(OUT)/tests/test_adapter: tests/test_adapter.c $(HAWK_LIB) $(LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ADAPTER_CPPFLAGS) $(ALL_CFLAGS) $< \
		$(HAWK_LIB) $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(OUT)/examples/%: examples/%.c $(LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

$(OUT)/examples/hawk_adapter: examples/hawk_adapter.c $(HAWK_LIB) $(LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ADAPTER_CPPFLAGS) $(ALL_CFLAGS) $< \
		$(HAWK_LIB) $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

test-fast: $(OUT)/tests/test_core $(OUT)/tests/test_adapter
	$(OUT)/tests/test_core
	$(OUT)/tests/test_adapter

test: $(TEST_BINS)
	@set -e; for test_bin in $(TEST_BINS); do $$test_bin; done

examples: $(EXAMPLE_BINS)

demo: $(EXAMPLE_BINS)
	$(OUT)/examples/sample
	$(OUT)/examples/hawk_adapter

$(OUT)/bench/bench: bench/bench.c $(LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

bench: $(OUT)/bench/bench
	$(OUT)/bench/bench

$(OUT)/tools/profile_oracle: tools/profile_oracle.c $(LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) $< $(LIB) $(LDFLAGS) -lmpfr -lgmp -o $@

oracle: $(OUT)/tools/profile_oracle
	$(OUT)/tools/profile_oracle

sanitize:
	$(MAKE) OUT=$(OUT)/sanitize \
		CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" \
		LDFLAGS="-fsanitize=address,undefined" test

check-clang:
	$(MAKE) OUT=$(OUT)/clang CC=clang test

cross-m4:
	$(MAKE) OUT=$(OUT)/cortex-m4 CC="$${M4_CC:-clang}" \
		AR="$${M4_AR:-llvm-ar}" \
		CFLAGS="-O2 --target=arm-none-eabi -mcpu=cortex-m4 -mthumb -ffreestanding" all

cross-rv32:
	$(MAKE) OUT=$(OUT)/rv32 CC="$${RV32_CC:-riscv64-linux-gnu-gcc}" \
		AR="$${RV32_AR:-riscv64-linux-gnu-ar}" \
		CFLAGS="-O2 -march=rv32im -mabi=ilp32 -ffreestanding" all

assembly: $(LIB) $(HAWK_LIB)
	@mkdir -p $(OUT)/analysis
	objdump -dr $(OUT)/src/masked.o > $(OUT)/analysis/masked.s
	objdump -dr $(OUT)/src/sampler.o > $(OUT)/analysis/sampler.s
	objdump -dr $(OUT)/adapters/hawk/pqsamp_hawk.o > $(OUT)/analysis/hawk_adapter.s

stack:
	$(MAKE) OUT=$(OUT)/stack CFLAGS="$(CFLAGS) -fstack-usage" all
	@find $(OUT)/stack -name '*.su' -type f -exec cat {} +

valgrind: $(TEST_BINS)
	@set -e; for test_bin in $(TEST_BINS); do \
		valgrind --error-exitcode=1 --leak-check=full --quiet $$test_bin; \
	done

fuzz:
	@mkdir -p $(OUT)
	clang $(CPPFLAGS) -std=c99 -O1 -g \
		-fsanitize=fuzzer,address,undefined $(SOURCES) fuzz/fuzz_sampler.c \
		-o $(OUT)/fuzz_sampler

install: all
	install -d "$(DESTDIR)$(PREFIX)/lib" "$(DESTDIR)$(PREFIX)/include"
	install -m 644 $(LIB) $(HAWK_LIB) "$(DESTDIR)$(PREFIX)/lib"
	install -m 644 include/pqsamp.h adapters/hawk/pqsamp_hawk.h \
		"$(DESTDIR)$(PREFIX)/include"

cbmc:
	cbmc proof/pack_harness.c src/bitslice.c $(CPPFLAGS) --unwind 33 \
		--unwinding-assertions --bounds-check --pointer-check \
		--signed-overflow-check

clean:
	rm -rf build

-include $(DEPS)
