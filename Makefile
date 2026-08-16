CC ?= cc
AR ?= ar
OUT ?= build

CPPFLAGS ?=
CPPFLAGS += -Iinclude -Isrc
HAWK_CPPFLAGS := -Iadapters/hawk
CFLAGS ?= -O2
WARN := -Wall -Wextra -Wpedantic -Werror -Wmissing-prototypes
WARN += -Wshadow -Wpointer-arith -Wredundant-decls -Wconversion
WARN += -Wsign-conversion
ALL_CFLAGS := $(CFLAGS) -std=c99 $(WARN)
LDFLAGS ?=
LDLIBS ?=

LIB := $(OUT)/libpqsamp.a
HAWK_LIB := $(OUT)/libpqsamp_hawk.a
SRC := src/rng.c src/profiles.c src/bitslice.c src/gadgets.c
SRC += src/maskaglia.c src/scalar.c src/masked.c
HAWK_SRC := adapters/hawk/pqsamp_hawk.c
OBJ := $(SRC:%.c=$(OUT)/%.o)
HAWK_OBJ := $(HAWK_SRC:%.c=$(OUT)/%.o)
DEP := $(OBJ:.o=.d) $(HAWK_OBJ:.o=.d)

TESTS := test_core test_sampler test_adapter
TEST_BIN := $(TESTS:%=$(OUT)/tests/%)

.PHONY: all adapter test bench oracle sanitize check-clang demo assembly
.PHONY: stack valgrind fuzz cbmc cross-m4 cross-rv32 clean

all: $(LIB)

adapter: $(HAWK_LIB)

$(LIB): $(OBJ)
	@mkdir -p $(@D)
	$(RM) $@
	$(AR) rcs $@ $^

$(HAWK_LIB): $(HAWK_OBJ)
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
	$(CC) $(CPPFLAGS) $(HAWK_CPPFLAGS) $(ALL_CFLAGS) $< \
		$(HAWK_LIB) $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

test: $(TEST_BIN)
	@set -e; for bin in $(TEST_BIN); do $$bin; done

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

$(OUT)/examples/sample: examples/sample.c $(LIB)
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ALL_CFLAGS) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

demo: $(OUT)/examples/sample
	$(OUT)/examples/sample

sanitize:
	$(MAKE) OUT=$(OUT)/sanitize \
		CFLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer" \
		LDFLAGS="-fsanitize=address,undefined" test

check-clang:
	$(MAKE) OUT=$(OUT)/clang CC=clang test

assembly: $(LIB) $(HAWK_LIB)
	@mkdir -p $(OUT)/analysis
	objdump -dr $(OUT)/src/gadgets.o > $(OUT)/analysis/gadgets.s
	objdump -dr $(OUT)/src/maskaglia.o > $(OUT)/analysis/maskaglia.s
	objdump -dr $(OUT)/src/masked.o > $(OUT)/analysis/masked.s
	objdump -dr $(OUT)/adapters/hawk/pqsamp_hawk.o \
		> $(OUT)/analysis/hawk_adapter.s

stack:
	$(MAKE) OUT=$(OUT)/stack CFLAGS="$(CFLAGS) -fstack-usage" all adapter
	@find $(OUT)/stack -name '*.su' -type f -exec cat {} +

valgrind: $(TEST_BIN)
	@set -e; for bin in $(TEST_BIN); do \
		valgrind --error-exitcode=1 --leak-check=full --quiet $$bin; \
	done

fuzz:
	@mkdir -p $(OUT)
	clang $(CPPFLAGS) -std=c99 -O1 -g \
		-fsanitize=fuzzer,address,undefined $(SRC) fuzz/fuzz_sampler.c \
		-o $(OUT)/fuzz_sampler

cbmc:
	cbmc proof/pack_harness.c src/bitslice.c $(CPPFLAGS) --unwind 33 \
		--unwinding-assertions --bounds-check --pointer-check \
		--signed-overflow-check

cross-m4:
	$(MAKE) OUT=$(OUT)/cortex-m4 CC="$${M4_CC:-clang}" \
		AR="$${M4_AR:-llvm-ar}" \
		CFLAGS="-O2 --target=arm-none-eabi -mcpu=cortex-m4 -mthumb -ffreestanding" \
		all adapter

cross-rv32:
	$(MAKE) OUT=$(OUT)/rv32 CC="$${RV32_CC:-riscv64-linux-gnu-gcc}" \
		AR="$${RV32_AR:-riscv64-linux-gnu-ar}" \
		CFLAGS="-O2 -march=rv32im -mabi=ilp32 -ffreestanding" all adapter

clean:
	$(RM) -r $(OUT)

-include $(DEP)
