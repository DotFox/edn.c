# EDN.C Makefile
# Supports x86_64 (SSE4.2, AVX2) and ARM64 (NEON) SIMD

CC ?= gcc
AR ?= ar
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
INCLUDES = -Iinclude -Isrc
LDFLAGS =

# Detect platform
UNAME_M := $(shell uname -m)
UNAME_S := $(shell uname -s)

# Platform-specific flags
ifeq ($(UNAME_M),arm64)
    # Apple Silicon M1/M2 - NEON enabled by default
    ARCH_FLAGS =
    ARCH = arm64
else ifeq ($(UNAME_M),aarch64)
    # ARM64 Linux
    ARCH_FLAGS =
    ARCH = arm64
else ifeq ($(UNAME_M),x86_64)
    # x86_64 - use SSE4.2 as baseline
    ARCH_FLAGS = -msse4.2
    ARCH = x86_64
else
    # Unknown architecture
    ARCH_FLAGS =
    ARCH = generic
endif

# Debug build
DEBUG ?= 0
ifeq ($(DEBUG),1)
    CFLAGS += -g -O0 -DDEBUG
    # Note: Sanitizers added only to LDFLAGS to avoid static library linking issues
    LDFLAGS += -fsanitize=address,undefined
else
    CFLAGS += -DNDEBUG
endif

# Verbose mode
VERBOSE ?= 0
ifeq ($(VERBOSE),1)
    Q =
else
    Q = @
endif

# Source files
SRCS = src/edn.c src/arena.c src/simd.c src/string.c src/number.c src/character.c src/identifier.c src/symbolic.c src/equality.c src/uniqueness.c src/collection.c src/tagged.c src/discard.c src/reader.c
OBJS = $(SRCS:.c=.o)

# Library output
LIB = libedn.a

# Test files
TEST_SRCS = $(wildcard test/*.c)
TEST_BINS = $(TEST_SRCS:.c=)

# Benchmark files
BENCH_SRCS = $(wildcard bench/*.c)
BENCH_BINS = $(BENCH_SRCS:.c=)

# Example files
EXAMPLES_SRCS = $(wildcard examples/*.c)
EXAMPLES_BINS = $(EXAMPLES_SRCS:.c=)

# Default target
.PHONY: all
all: $(LIB)
	@echo "✓ Build complete: $(LIB)"
	@echo "  Run 'make test' to run tests"
	@echo "  Run 'make info' to see build configuration"

# Build library
$(LIB): $(OBJS)
	@echo "  AR      $@"
	$(Q)$(AR) rcs $@ $^

# Compile source files
%.o: %.c
	@echo "  CC      $@"
	$(Q)$(CC) $(CFLAGS) $(ARCH_FLAGS) $(INCLUDES) -c $< -o $@

# Build and run tests
.PHONY: test
test: $(TEST_BINS)
	@echo "Running tests..."
	$(Q)for test in $(TEST_BINS); do \
		echo ""; \
		./$$test || exit 1; \
	done

# Build individual test
test/%: test/%.c $(LIB)
	@echo "  CC      $@"
	$(Q)$(CC) $(CFLAGS) $(ARCH_FLAGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) -o $@

# Build and run quick benchmark
.PHONY: bench
bench: bench/bench_integration
	@echo "Running quick benchmark..."
	@echo ""
	@./bench/bench_integration

# Build and run Clojure benchmarks (clojure.edn and fast-edn)
.PHONY: bench-clj
bench-clj:
	@echo "Running Clojure benchmarks..."
	@echo ""
	@clojure -M -m bench-integration

# Build and run comparison benchmarks (C, then Clojure)
.PHONY: bench-compare
bench-compare: bench/bench_integration
	@echo "Running C benchmark..."
	@echo ""
	@./bench/bench_integration
	@echo ""
	@echo "================================================================================"
	@echo ""
	@echo "Running Clojure benchmarks..."
	@echo ""
	@clojure -M -m bench-integration

# Build and run all benchmarks
.PHONY: bench-all
bench-all: $(BENCH_BINS)
	@echo "Running all benchmarks..."
	$(Q)for benchmark in $(BENCH_BINS); do \
		echo ""; \
		./$$benchmark || exit 1; \
	done

# Build benchmarks only
.PHONY: bench-build
bench-build: $(BENCH_BINS)

bench/%: bench/%.c $(LIB)
	@echo "  CC      $@"
	$(Q)$(CC) $(CFLAGS) $(ARCH_FLAGS) $(INCLUDES) -D_POSIX_C_SOURCE=200809L -O3 -g $< $(LIB) $(LDFLAGS) -o $@

# Debug build
.PHONY: debug
debug:
	$(MAKE) DEBUG=1

# Clean build artifacts
.PHONY: clean
clean:
	@echo "  CLEAN"
	$(Q)rm -f $(OBJS) $(LIB)
	$(Q)rm -f $(TEST_BINS)
	$(Q)rm -f $(BENCH_BINS)
	$(Q)rm -f $(EXAMPLES_BINS)
	$(Q)rm -rf *.dSYM test/*.dSYM bench/*.dSYM
	$(Q)rm -rf profile_*.trace

# Print configuration
.PHONY: info
info:
	@echo "EDN.C Build Configuration"
	@echo "========================="
	@echo "Architecture: $(ARCH)"
	@echo "Compiler:     $(CC)"
	@echo "CFLAGS:       $(CFLAGS) $(ARCH_FLAGS)"
	@echo "Debug:        $(DEBUG)"
	@echo "Sources:      $(SRCS)"
	@echo "Tests:        $(TEST_SRCS)"
	@echo "Benchmarks:   $(BENCH_SRCS)"

# Help
# Format all source files with clang-format
.PHONY: format
format:
	@echo "  FORMAT  all C files"
	$(Q)find src include test bench -name '*.[ch]' -exec clang-format -i {} +

# Check formatting without modifying files
.PHONY: format-check
format-check:
	@echo "  FORMAT-CHECK"
	$(Q)find src include test bench -name '*.[ch]' -exec clang-format --dry-run -Werror {} +

# Generate compile_commands.json for LSP (requires bear or compiledb)
.PHONY: compile-commands
compile-commands: clean
	@echo "  GEN     compile_commands.json"
	@if command -v bear >/dev/null 2>&1; then \
		bear -- $(MAKE) all test; \
	elif command -v compiledb >/dev/null 2>&1; then \
		compiledb make all test; \
	else \
		echo "Error: Please install 'bear' or 'compiledb' to generate compile_commands.json"; \
		echo "  macOS: brew install bear"; \
		echo "  Linux: apt-get install bear (or pip install compiledb)"; \
		exit 1; \
	fi

.PHONY: help
help:
	@echo "EDN.C Makefile targets:"
	@echo "  make                  - Build library ($(LIB))"
	@echo "  make test             - Build and run all tests"
	@echo "  make bench            - Build and run quick benchmark (C integration)"
	@echo "  make bench-clj        - Run Clojure benchmarks (clojure.edn and fast-edn)"
	@echo "  make bench-compare    - Run C and Clojure benchmarks for comparison"
	@echo "  make bench-all        - Build and run all C benchmarks"
	@echo "  make bench-build      - Build benchmarks only (don't run)"
	@echo "  make debug            - Build with debug symbols and sanitizers"
	@echo "  make format           - Format all C files with clang-format"
	@echo "  make format-check     - Check formatting without modifying files"
	@echo "  make compile-commands - Generate compile_commands.json for LSP"
	@echo "  make clean            - Remove build artifacts"
	@echo "  make info             - Print build configuration"
	@echo "  make help             - Show this help message"
	@echo ""
	@echo "Options:"
	@echo "  DEBUG=1               - Enable debug build"
	@echo "  VERBOSE=1             - Show full compiler commands"

.DEFAULT_GOAL := all
