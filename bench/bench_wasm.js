/**
 * WebAssembly Integration Benchmarks
 *
 * Benchmarks EDN.C WebAssembly bindings with realistic workloads.
 * Compares against native Node.js JSON parsing where applicable.
 */

const Module = require("../edn.js");
const fs = require("fs");
const path = require("path");

// Benchmark configuration
// Note: Very long durations (>2000ms) may exhaust Emval handles due to accumulation
const MIN_DURATION_MS = parseInt(process.env.MIN_DURATION_MS || "500", 10);
const MIN_ITERATIONS = parseInt(process.env.MIN_ITERATIONS || "100", 10);
const WARMUP_ITERATIONS = 3;

/**
 * Get current time in nanoseconds
 */
function getTimeNs() {
    const [seconds, nanoseconds] = process.hrtime();
    return BigInt(seconds) * 1000000000n + BigInt(nanoseconds);
}

/**
 * Run a benchmark function
 */
function benchRun(name, data, benchFn, options = {}) {
    const minDurationNs =
        BigInt(options.minDurationMs || MIN_DURATION_MS) * 1000000n;
    const minIterations = options.minIterations || MIN_ITERATIONS;

    // Warmup
    for (let i = 0; i < WARMUP_ITERATIONS; i++) {
        benchFn(data);
    }

    // Force GC after warmup if available
    if (global.gc) {
        global.gc();
    }

    // Collect samples
    const samples = [];
    let iterations = 0;
    const startTime = getTimeNs();
    let elapsed = 0n;

    while (elapsed < minDurationNs || iterations < minIterations) {
        const iterStart = getTimeNs();
        benchFn(data);
        const iterEnd = getTimeNs();

        samples.push(Number(iterEnd - iterStart));
        iterations++;

        // Periodic GC to prevent memory buildup (every 50 iterations)
        // if (global.gc && iterations % 50 === 0) {
        //     global.gc();
        // }

        elapsed = getTimeNs() - startTime;
    }

    // Calculate statistics
    const totalTimeNs = Number(elapsed);
    const meanTimeUs = totalTimeNs / iterations / 1000;

    // Calculate standard deviation
    const sampleMean = samples.reduce((a, b) => a + b, 0) / samples.length;
    const variance =
        samples.reduce((acc, val) => {
            const diff = val - sampleMean;
            return acc + diff * diff;
        }, 0) /
        (samples.length - 1);
    const stddevTimeUs = Math.sqrt(variance) / 1000;

    // 95% confidence interval
    const confidenceIntervalUs =
        (1.96 * stddevTimeUs) / Math.sqrt(samples.length);

    // Throughput in GB/s
    const totalBytes = iterations * data.length;
    const timeSeconds = totalTimeNs / 1e9;
    const throughputGBps = totalBytes / timeSeconds / (1024 * 1024 * 1024);

    return {
        name,
        iterations,
        totalTimeMs: totalTimeNs / 1e6,
        meanTimeUs,
        stddevTimeUs,
        confidenceIntervalUs,
        throughputGBps,
        dataSize: data.length,
    };
}

/**
 * Format number with thousands separator
 */
function formatNumber(num) {
    return num.toString().replace(/\B(?=(\d{3})+(?!\d))/g, ",");
}

/**
 * Print benchmark result
 */
function printResult(result) {
    const iterations = formatNumber(result.iterations).padStart(14);
    const totalTime = result.totalTimeMs.toFixed(2).padStart(10);
    const meanTime = result.meanTimeUs.toFixed(3).padStart(10);
    const confidence = result.confidenceIntervalUs.toFixed(3).padStart(7);
    const throughput = result.throughputGBps.toFixed(3).padStart(5);
    const size = `(${formatBytes(result.dataSize)})`;

    console.log(
        `${result.name.padEnd(40)} ` +
            `${iterations}  ` +
            `${totalTime}  ` +
            `${meanTime} ± ${confidence}  ` +
            `${throughput} GB/s  ` +
            `${size}`,
    );
}

/**
 * Print benchmark header
 */
function printHeader() {
    console.log(
        "Benchmark".padEnd(40) +
            " " +
            "Iterations".padStart(14) +
            "  " +
            "Total (ms)".padStart(10) +
            "  " +
            "Mean (μs)".padStart(21) +
            "  " +
            "Throughput".padStart(10) +
            "  " +
            "Size",
    );
    console.log("-".repeat(120));
}

/**
 * Format bytes to human readable
 */
function formatBytes(bytes) {
    if (bytes < 1024) return `${bytes} bytes`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${(bytes / 1024 / 1024).toFixed(1)} MB`;
}

/**
 * Read file
 */
function readFile(filename) {
    const filepath = path.join(__dirname, "data", filename);
    try {
        return fs.readFileSync(filepath, "utf8");
    } catch (err) {
        console.error(`Failed to read ${filename}: ${err.message}`);
        return null;
    }
}

/**
 * Benchmark a single file
 */
function benchFile(filename, description, mode, ccall) {
    const data = readFile(filename);
    if (!data) {
        console.log(`${description.padEnd(40)} FAILED (could not read file)`);
        return null;
    }

    let benchFn;
    if (mode === "parse-only") {
        // Parse-only: just measure the parse call
        benchFn = (input) => {
            const handle = ccall(
                "wasm_edn_parse_to_js",
                "number",
                ["string"],
                [input],
            );
            if (handle === 0) {
                throw new Error("Parse failed");
            }
            // Convert to value
            const value = Module.Emval.toValue(handle);
            return value;
        };
    } else if (mode === "roundtrip") {
        // Roundtrip: parse + full conversion to JS types
        benchFn = (input) => {
            const handle = ccall(
                "wasm_edn_parse_to_js",
                "number",
                ["string"],
                [input],
            );
            if (handle === 0) {
                throw new Error("Parse failed");
            }
            const value = Module.Emval.toValue(handle);
            // Force conversion of nested structures
            if (value instanceof Map) {
                // Touch all map entries to force conversion
                for (const [k, v] of value.entries()) {
                    // Access forces conversion
                }
            }
            return value;
        };
    } else if (mode === "json") {
        // Convert EDN to JSON-like format for comparison
        benchFn = (input) => {
            // This is a rough approximation - EDN has features JSON doesn't
            const jsonLike = input
                .replace(/:/g, '"') // Keywords to strings (rough)
                .replace(/\s+/g, " "); // Normalize whitespace
            return JSON.parse(jsonLike);
        };
    }

    try {
        const result = benchRun(description, data, benchFn);
        printResult(result);
        return result;
    } catch (err) {
        console.log(`${description.padEnd(40)} FAILED (${err.message})`);
        return null;
    }
}

/**
 * Main benchmark suite
 */
Module.onRuntimeInitialized = () => {
    const ccall = Module.ccall.bind(Module);

    console.log("EDN.C WebAssembly Integration Benchmarks");
    console.log("=========================================\n");
    console.log(`Node.js: ${process.version}`);
    console.log(`Platform: ${process.platform} ${process.arch}`);
    console.log(`WASM SIMD: enabled`);
    console.log(
        `GC exposed: ${global.gc ? "yes" : "no (run with --expose-gc for better memory management)"}\n`,
    );

    // Parse-only mode
    console.log("=== PARSE-ONLY MODE (Pure Parsing Performance) ===");
    console.log("Measures only parsing time, returns Emval handle\n");
    printHeader();

    console.log("\n--- Basic Maps ---");
    benchFile("basic_10.edn", "basic_10", "parse-only", ccall);
    benchFile("basic_100.edn", "basic_100", "parse-only", ccall);
    benchFile("basic_1000.edn", "basic_1000", "parse-only", ccall);
    benchFile("basic_10000.edn", "basic_10000", "parse-only", ccall);
    benchFile("basic_100000.edn", "basic_100000", "parse-only", ccall);

    console.log("\n--- Keyword Vectors ---");
    benchFile("keywords_10.edn", "keywords_10", "parse-only", ccall);
    benchFile("keywords_100.edn", "keywords_100", "parse-only", ccall);
    benchFile("keywords_1000.edn", "keywords_1000", "parse-only", ccall);
    benchFile("keywords_10000.edn", "keywords_10000", "parse-only", ccall);

    console.log("\n--- Integer Arrays ---");
    benchFile("ints_1400.edn", "ints_1400", "parse-only", ccall);

    console.log("\n--- String Collections ---");
    benchFile("strings_1000.edn", "strings_1000", "parse-only", ccall);
    benchFile("strings_uni_250.edn", "strings_uni_250", "parse-only", ccall);

    console.log("\n--- Nested Structures ---");
    benchFile("nested_100000.edn", "nested_100000", "parse-only", ccall);

    // Roundtrip mode
    console.log("\n\n=== ROUNDTRIP MODE (Parse + JS Conversion) ===");
    console.log("Measures parsing + conversion to JavaScript objects\n");
    printHeader();

    console.log("\n--- Basic Maps ---");
    benchFile("basic_10.edn", "basic_10", "roundtrip", ccall);
    benchFile("basic_100.edn", "basic_100", "roundtrip", ccall);
    benchFile("basic_1000.edn", "basic_1000", "roundtrip", ccall);
    benchFile("basic_10000.edn", "basic_10000", "roundtrip", ccall);
    benchFile("basic_100000.edn", "basic_100000", "roundtrip", ccall);

    console.log("\n--- Keyword Vectors ---");
    benchFile("keywords_10.edn", "keywords_10", "roundtrip", ccall);
    benchFile("keywords_100.edn", "keywords_100", "roundtrip", ccall);
    benchFile("keywords_1000.edn", "keywords_1000", "roundtrip", ccall);
    benchFile("keywords_10000.edn", "keywords_10000", "roundtrip", ccall);

    console.log("\n--- Integer Arrays ---");
    benchFile("ints_1400.edn", "ints_1400", "roundtrip", ccall);

    console.log("\n--- String Collections ---");
    benchFile("strings_1000.edn", "strings_1000", "roundtrip", ccall);
    benchFile("strings_uni_250.edn", "strings_uni_250", "roundtrip", ccall);

    console.log("\n--- Nested Structures ---");
    benchFile("nested_100000.edn", "nested_100000", "roundtrip", ccall);

    console.log("\n");
    console.log("Notes:");
    console.log("  - Parse-only: Measures pure parsing, returns Emval handle");
    console.log(
        "  - Roundtrip: Includes parsing + conversion to JS objects (Map, Set, Symbol, etc.)",
    );
    console.log("  - Each benchmark runs for minimum 500ms or 100 iterations");
    console.log(
        `  - Warmup: ${WARMUP_ITERATIONS} iterations before measurement`,
    );
    console.log(
        "  - GB/s calculated as: (iterations × file_size) / time / 1024³",
    );
    console.log("  - Mean ± 95% confidence interval shown in microseconds");
};
