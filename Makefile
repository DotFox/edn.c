# EDN.C Makefile
# Supports x86_64 (SSE4.2, AVX2) and ARM64 (NEON) SIMD
# WebAssembly support via Emscripten

# Native build tools
CC ?= gcc
AR ?= ar

# WebAssembly build tools (Emscripten)
EMCC ?= emcc
EMAR ?= emar

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

# Platform-specific libraries
ifeq ($(UNAME_S),Linux)
    LDLIBS = -lm
else
    LDLIBS =
endif

# Debug build
DEBUG ?= 0
ifeq ($(DEBUG),1)
    CFLAGS += -g -O0 -DDEBUG
    # Note: Sanitizers added only to LDFLAGS to avoid static library linking issues
    LDFLAGS += -fsanitize=address,leak,undefined
else
    CFLAGS += -DNDEBUG
endif

# Clojure extension features (disabled by default)
# Includes: map namespace syntax, extended characters, metadata, ratio, extended integers
CLOJURE_EXTENSION ?= 0
ifneq (,$(filter 1,$(CLOJURE_EXTENSION) $(ALL)))
    CFLAGS += -DEDN_ENABLE_CLOJURE_EXTENSION
endif

# Experimental extension features (disabled by default)
# Includes: text blocks, underscore in numeric literals
EXPERIMENTAL_EXTENSION ?= 0
ifneq (,$(filter 1,$(EXPERIMENTAL_EXTENSION) $(ALL)))
    CFLAGS += -DEDN_ENABLE_EXPERIMENTAL_EXTENSION
endif

# Verbose mode
VERBOSE ?= 0
ifeq ($(VERBOSE),1)
    Q =
else
    Q = @
endif

# Source files
SRCS = src/edn.c src/arena.c src/simd.c src/string.c src/number.c src/character.c src/identifier.c src/symbolic.c src/equality.c src/uniqueness.c src/collection.c src/tagged.c src/discard.c src/reader.c src/metadata.c src/newline_finder.c

# Native build objects and library
OBJS = $(SRCS:.c=.o)
LIB = libedn.a

# WebAssembly build objects and outputs
WASM_SRCS = $(SRCS) bindings/wasm/wasm_edn.c
WASM_OBJS = $(WASM_SRCS:.c=.wasm.o)
WASM_LIB = libedn.wasm.a
WASM_MODULE = edn.wasm
WASM_JS = edn.js

# WebAssembly SIMD128 flags
WASM_COMPILE_FLAGS = -msimd128 -mbulk-memory
WASM_LINK_FLAGS = -msimd128 -mbulk-memory
WASM_LINK_FLAGS += -s EXPORTED_RUNTIME_METHODS=ccall,cwrap,Emval
WASM_LINK_FLAGS += -s ALLOW_MEMORY_GROWTH=1
WASM_LINK_FLAGS += -s INITIAL_MEMORY=33554432
WASM_LINK_FLAGS += -s MAXIMUM_MEMORY=4294967296
WASM_LINK_FLAGS += -s EXPORT_ALL=0
WASM_LINK_FLAGS += -lembind
WASM_LINK_FLAGS += -s STACK_SIZE=5242880

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
	$(Q)$(CC) $(CFLAGS) $(ARCH_FLAGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@
	@./$@

# Build and run quick benchmark
.PHONY: bench
bench: bench/bench_integration
	@./bench/bench_integration

# Run Clojure benchmarks (clojure.edn and fast-edn)
.PHONY: bench-clj
bench-clj:
	@clojure -M -m bench-integration

# Build and run comparison benchmarks (C, then Clojure)
.PHONY: bench-compare
bench-compare: bench bench-clj

# Build and run WebAssembly benchmarks
.PHONY: bench-wasm
bench-wasm: wasm
	@echo "Running WebAssembly benchmarks..."
	@node --expose-gc bench/bench_wasm.js

# Build and run all benchmarks (C + WASM)
.PHONY: bench-all
bench-all: $(BENCH_BINS) bench-wasm
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
	$(Q)$(CC) $(CFLAGS) $(ARCH_FLAGS) $(INCLUDES) -D_POSIX_C_SOURCE=200809L -O3 $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

# Build examples
.PHONY: examples
examples: $(EXAMPLES_BINS)

examples/%: examples/%.c $(LIB)
	@echo "  CC      $@"
	$(Q)$(CC) $(CFLAGS) $(ARCH_FLAGS) $(INCLUDES) $< $(LIB) $(LDFLAGS) $(LDLIBS) -o $@

# Build CLI tool
.PHONY: cli
cli: examples/edn_cli

# Build TUI tool
.PHONY: tui
tui: examples/edn_tui

# Debug build
.PHONY: debug
debug:
	$(MAKE) DEBUG=1

# WebAssembly targets
.PHONY: wasm
wasm: $(WASM_MODULE)
	@echo "✓ WebAssembly build complete: $(WASM_MODULE), $(WASM_JS)"
	@echo "  SIMD128 support enabled"
	@echo ""
	@echo "Files created in project root:"
	@echo "  - edn.wasm (WebAssembly module)"
	@echo "  - edn.js (JavaScript loader)"
	@echo ""
	@echo "To test in Node.js, run:"
	@echo "  node examples/wasm_example.js"

# Build WebAssembly module with JavaScript glue code
$(WASM_MODULE): $(WASM_OBJS)
	@echo "  EMCC    $@"
	$(Q)$(EMCC) -std=gnu11 -O2 $(WASM_LINK_FLAGS) $(INCLUDES) $(WASM_OBJS) -o $(WASM_JS)

# Build WebAssembly static library
$(WASM_LIB): $(WASM_OBJS)
	@echo "  EMAR    $@"
	$(Q)$(EMAR) rcs $@ $^

# Compile WebAssembly object files
%.wasm.o: %.c
	@echo "  EMCC    $@"
	$(Q)$(EMCC) -std=gnu11 -Wall -Wextra -Wpedantic -O2 \
		-Wno-dollar-in-identifier-extension \
		-Wno-gnu-zero-variadic-macro-arguments \
		$(filter -D%, $(CFLAGS)) $(WASM_COMPILE_FLAGS) $(INCLUDES) -c $< -o $@

# Clean build artifacts
.PHONY: clean
clean:
	@echo "  CLEAN"
	$(Q)rm -f $(OBJS) $(LIB)
	$(Q)rm -f $(WASM_OBJS) $(WASM_LIB) $(WASM_MODULE) $(WASM_JS) edn.wasm.map
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
	@echo ""
	@echo "WebAssembly Configuration"
	@echo "========================="
	@echo "Compiler:     $(EMCC)"
	@echo "CFLAGS:       $(CFLAGS) $(WASM_FLAGS)"
	@echo "WASM Sources: $(WASM_SRCS)"

.PHONY: info-wasm
info-wasm:
	@echo "EDN.C WebAssembly Build Configuration"
	@echo "======================================"
	@echo "Compiler:     $(EMCC)"
	@echo "CFLAGS:       $(CFLAGS) $(WASM_FLAGS)"
	@echo "SIMD:         SIMD128 enabled"
	@echo "Sources:      $(WASM_SRCS)"
	@echo ""
	@echo "Optional features:"
	@echo "  CLOJURE_EXTENSION:        $(CLOJURE_EXTENSION)"
	@echo "  EXPERIMENTAL_EXTENSION:   $(EXPERIMENTAL_EXTENSION)"

# Help
# Format all source files with clang-format
.PHONY: format
format:
	@echo "  FORMAT  all C files"
	$(Q)find src include test bench examples bindings -name '*.[ch]' -exec clang-format -i {} +

# Check formatting without modifying files
.PHONY: format-check
format-check:
	@echo "  FORMAT-CHECK"
	$(Q)find src include test bench examples bindings -name '*.[ch]' -exec clang-format --dry-run -Werror {} +

# Generate compile_commands.json and .clangd for LSP
.PHONY: compile-commands
compile-commands: clean
	@echo "  GEN     compile_commands.json + .clangd"
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
	@./generate_clangd.sh

# Generate .clangd configuration with auto-detected Emscripten paths
.PHONY: clangd
clangd:
	@./generate_clangd.sh

.PHONY: help
help:
	@echo "EDN.C Makefile targets:"
	@echo ""
	@echo "Native builds:"
	@echo "  make                  - Build library ($(LIB))"
	@echo "  make test             - Build and run all tests"
	@echo "  make cli              - Build CLI tool (examples/edn_cli)"
	@echo "  make tui              - Build TUI viewer (examples/edn_tui)"
	@echo "  make examples         - Build all example programs"
	@echo "  make bench            - Build and run quick benchmark (C integration)"
	@echo "  make bench-clj        - Run Clojure benchmarks (clojure.edn and fast-edn)"
	@echo "  make bench-compare    - Run C and Clojure benchmarks for comparison"
	@echo "  make bench-all        - Build and run all benchmarks (C + WASM)"
	@echo "  make bench-build      - Build benchmarks only (don't run)"
	@echo "  make debug            - Build with debug symbols and sanitizers"
	@echo ""
	@echo "WebAssembly builds:"
	@echo "  make wasm             - Build WebAssembly module ($(WASM_MODULE))"
	@echo "  make bench-wasm       - Build and run WebAssembly benchmarks"
	@echo "  make info-wasm        - Print WebAssembly build configuration"
	@echo ""
	@echo "Development tools:"
	@echo "  make format           - Format all C files with clang-format"
	@echo "  make format-check     - Check formatting without modifying files"
	@echo "  make compile-commands - Generate compile_commands.json + .clangd for LSP"
	@echo "  make clangd           - Regenerate .clangd configuration only"
	@echo "  make clean            - Remove all build artifacts"
	@echo "  make info             - Print build configuration (native + WASM)"
	@echo "  make help             - Show this help message"
	@echo ""
	@echo "Options (apply to both native and WASM builds):"
	@echo "  CLOJURE_EXTENSION=1        - Enable Clojure extensions (ratio, extended integers, metadata, etc.)"
	@echo "  EXPERIMENTAL_EXTENSION=1   - Enable experimental features (text blocks, underscores in numbers)"
	@echo "  ALL=1                       - Enable all features at once"
	@echo "  VERBOSE=1                   - Show full compiler commands"
	@echo "  DEBUG=1                     - Enable debug build (native only)"
	@echo ""
	@echo "WebAssembly requirements:"
	@echo "  - Emscripten SDK (https://emscripten.org/)"
	@echo "  - Node.js with WASM SIMD support (v16.4+ or use --experimental-wasm-simd)"

.DEFAULT_GOAL := all
