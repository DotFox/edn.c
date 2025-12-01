// Example usage of EDN.C WebAssembly bindings

const Module = require("../edn.js");

Module.onRuntimeInitialized = () => {
    console.log("EDN.C WebAssembly module loaded");
    console.log("SIMD128 support enabled\n");

    // Example EDN strings to parse
    const examples = [
        // Primitives
        { edn: "nil", desc: "Nil value" },
        { edn: "true", desc: "Boolean" },
        { edn: "42", desc: "Integer" },
        { edn: "9007199254740992", desc: "Large integer (BigInt)" },
        { edn: "3.14159", desc: "Float" },
        { edn: "22/7", desc: "Ratio", skip: !Module._wasm_edn_parse_to_js }, // Only if RATIO enabled
        { edn: "99999999999999999999/88888888888888888888", desc: "BigRatio", skip: !Module._wasm_edn_parse_to_js },
        { edn: "-99999999999999999999/88888888888888888888", desc: "BigRatio", skip: !Module._wasm_edn_parse_to_js },
        { edn: "99999999999999999999/1", desc: "BigRatio", skip: !Module._wasm_edn_parse_to_js },
        { edn: "\\a", desc: "Character" },
        { edn: '"hello world"', desc: "String" },

        { edn: "1234_5678_9012_3456", desc: "Long credit card number" },
        { edn: "3.14_15", desc: "Pi" },
        { edn: "0xFF_EC_DE_5E", desc: "HEX bytes" },

        { edn: "0000042", desc: "Leading zeros in octal integer" },
        { edn: "00000.5", desc: "leading zeros in float" },
        { edn: "0777/2", desc: "Strange clojure compatibility" },

        // Symbols and keywords
        { edn: "my-symbol", desc: "Symbol" },
        { edn: "ns/qualified-symbol", desc: "Namespaced symbol" },
        { edn: ":keyword", desc: "Keyword" },
        { edn: ":ns/qualified-keyword", desc: "Namespaced keyword" },

        // Collections
        { edn: "[1 2 3]", desc: "Vector" },
        { edn: "(1 2 3)", desc: "List" },
        { edn: "#{1 2 3}", desc: "Set" },
        { edn: '{:name "Alice" :age 30}', desc: "Map" },

        // Nested structures
        { edn: "[1 [2 [3 [4]]]]", desc: "Nested vectors" },
        { edn: '{:user {:name "Bob" :scores [95 87 92]}}', desc: "Nested map" },

        // Tagged literals
        { edn: '#inst "2024-01-15"', desc: "Tagged literal (inst)" },
        {
            edn: '#uuid "550e8400-e29b-41d4-a716-446655440000"',
            desc: "Tagged literal (uuid)",
        },
        { edn: "#myapp/custom {:data 42}", desc: "Custom tagged literal" },
    ];

    console.log("=".repeat(70));
    console.log("EDN to JavaScript Conversion Examples");
    console.log("=".repeat(70));

    for (const example of examples) {
        if (example.skip) continue;

        console.log(`\n${example.desc}:`);
        console.log(`  EDN:  ${example.edn}`);

        try {
            // Parse EDN and convert to JavaScript
            const jsValue = Module.ccall(
                "wasm_edn_parse_to_js",
                "number", // Returns EM_VAL handle
                ["string"],
                [example.edn],
            );

            if (jsValue !== 0) {
                const value = Module.Emval.toValue(jsValue);
                console.log(`  JS:   ${formatJSValue(value)}`);
                console.log(`  Type: ${getJSType(value)}`);

                // Clean up Emval handle - no cleanup needed, Emval handles it
            } else {
                console.log(`  Error: Failed to parse`);
            }
        } catch (e) {
            console.log(`  Error: ${e.message}`);
        }
    }

    console.log("\n" + "=".repeat(70));
    console.log("Advanced Examples");
    console.log("=".repeat(70));

    // Example: Working with Maps
    console.log("\n📦 Map Example:");
    const mapEDN = '{:name "Charlie" :age 25 :hobbies ["reading" "coding"]}';
    const mapHandle = Module.ccall(
        "wasm_edn_parse_to_js",
        "number",
        ["string"],
        [mapEDN],
    );
    if (mapHandle) {
        const map = Module.Emval.toValue(mapHandle);
        console.log(`  Original: ${mapEDN}`);
        console.log(`  JS Map size: ${map.size}`);
        console.log(`  Keys:`, Array.from(map.keys()).map(formatJSValue));
        console.log(`  Name: ${formatJSValue(map.get(Symbol.for(":name")))}`);
    }

    // Example: Working with Sets
    console.log("\n📦 Set Example:");
    const setEDN = "#{:red :green :blue}";
    const setHandle = Module.ccall(
        "wasm_edn_parse_to_js",
        "number",
        ["string"],
        [setEDN],
    );
    if (setHandle) {
        const set = Module.Emval.toValue(setHandle);
        console.log(`  Original: ${setEDN}`);
        console.log(`  JS Set size: ${set.size}`);
        console.log(`  Values:`, Array.from(set).map(formatJSValue));
    }

    // Example: BigInt handling
    console.log("\n📦 BigInt Example:");
    const bigintEDN = "999999999999999999999999999999";
    const bigintHandle = Module.ccall(
        "wasm_edn_parse_to_js",
        "number",
        ["string"],
        [bigintEDN],
    );
    if (bigintHandle) {
        const bigint = Module.Emval.toValue(bigintHandle);
        console.log(`  Original: ${bigintEDN}`);
        console.log(`  JS Value: ${bigint}`);
        console.log(`  Type: ${typeof bigint}`);
    }

    // =========================================================================
    // Custom JavaScript Readers
    // =========================================================================
    console.log("\n" + "=".repeat(70));
    console.log("Custom JavaScript Readers");
    console.log("=".repeat(70));

    // Register custom readers for tagged literals
    console.log("\nRegistering custom readers...");

    // Reader for #inst - convert ISO date strings to Date objects
    const instReader = (value) => {
        console.log(`  [inst reader] Received: ${value}`);
        return new Date(value);
    };

    // Reader for #uuid - wrap in a UUID object
    const uuidReader = (value) => {
        console.log(`  [uuid reader] Received: ${value}`);
        return { type: "UUID", value: value, toString: () => `UUID(${value})` };
    };

    // Reader for #point - convert [x, y] vector to Point object
    const pointReader = (value) => {
        console.log(`  [point reader] Received:`, value);
        if (Array.isArray(value) && value.length === 2) {
            return { type: "Point", x: value[0], y: value[1] };
        }
        return null;
    };

    // Reader for #json - parse nested JSON strings
    const jsonReader = (value) => {
        console.log(`  [json reader] Received: ${value}`);
        try {
            return JSON.parse(value);
        } catch (e) {
            return null;
        }
    };

    // Register the readers
    const registerReader = (tag, callback) => {
        const callbackHandle = Module.Emval.toHandle(callback);
        const result = Module.ccall(
            "wasm_edn_register_reader",
            "number",
            ["string", "number"],
            [tag, callbackHandle],
        );
        console.log(`  Registered reader for #${tag}: ${result ? "✓" : "✗"}`);
        return result;
    };

    registerReader("inst", instReader);
    registerReader("uuid", uuidReader);
    registerReader("point", pointReader);
    registerReader("json", jsonReader);

    const readerCount = Module.ccall("wasm_edn_reader_count", "number", [], []);
    console.log(`  Total registered readers: ${readerCount}`);

    console.log("\nParsing with custom readers:");

    const readerExamples = [
        {
            edn: '#inst "2024-03-15T10:30:00Z"',
            desc: "Date parsing with #inst reader",
        },
        {
            edn: '#uuid "550e8400-e29b-41d4-a716-446655440000"',
            desc: "UUID parsing with #uuid reader",
        },
        {
            edn: "#point [10 20]",
            desc: "Point parsing with #point reader",
        },
        {
            edn: '#json "{\\"name\\":\\"test\\",\\"value\\":42}"',
            desc: "JSON parsing with #json reader",
        },
        {
            edn: '{:created #inst "2024-01-01" :location #point [100 200]}',
            desc: "Multiple readers in one document",
        },
    ];

    for (const example of readerExamples) {
        console.log(`\n${example.desc}:`);
        console.log(`  EDN: ${example.edn}`);

        try {
            // Parse with readers (default_mode = 0 = PASSTHROUGH)
            const jsValue = Module.ccall(
                "wasm_edn_parse_to_js_with_readers",
                "number",
                ["string", "number"],
                [example.edn, 0],
            );

            if (jsValue !== 0) {
                const value = Module.Emval.toValue(jsValue);
                console.log(`  Result: ${formatJSValue(value)}`);
                console.log(`  Type: ${getJSType(value)}`);

                if (value instanceof Date) {
                    console.log(`  ISO: ${value.toISOString()}`);
                }
                if (value && value.type === "Point") {
                    console.log(`  Coordinates: (${value.x}, ${value.y})`);
                }
            } else {
                console.log(`  Error: Failed to parse`);
            }
        } catch (e) {
            console.log(`  Error: ${e.message}`);
        }
    }

    console.log("\nUnregistering #point reader...");
    Module.ccall("wasm_edn_unregister_reader", null, ["string"], ["point"]);
    const newCount = Module.ccall("wasm_edn_reader_count", "number", [], []);
    console.log(`  Readers remaining: ${newCount}`);

    // Parse #point without reader (should return tagged literal)
    console.log("\nParsing #point without reader (passthrough mode):");
    const pointEDN = "#point [30 40]";
    const pointHandle = Module.ccall(
        "wasm_edn_parse_to_js_with_readers",
        "number",
        ["string", "number"],
        [pointEDN, 0], // 0 = PASSTHROUGH mode
    );
    if (pointHandle) {
        const value = Module.Emval.toValue(pointHandle);
        console.log(`  EDN: ${pointEDN}`);
        console.log(`  Result: ${formatJSValue(value)}`);
        console.log(`  (Note: Returns {tag, value} object in passthrough mode)`);
    }

    console.log("\nClearing all readers...");
    Module.ccall("wasm_edn_clear_readers", null, [], []);
    const finalCount = Module.ccall("wasm_edn_reader_count", "number", [], []);
    console.log(`  Readers remaining: ${finalCount}`);

    console.log("\n✅ All examples completed!\n");
};

// Helper functions
function formatJSValue(value) {
    if (value === null) return "null";
    if (value === undefined) return "undefined";
    if (typeof value === "symbol") return value.toString();
    if (value instanceof Date) return `Date(${value.toISOString()})`;
    if (value instanceof Map) {
        const entries = Array.from(value.entries())
            .map(([k, v]) => `${formatJSValue(k)}: ${formatJSValue(v)}`)
            .join(", ");
        return `Map { ${entries} }`;
    }
    if (value instanceof Set) {
        const values = Array.from(value).map(formatJSValue).join(", ");
        return `Set { ${values} }`;
    }
    if (Array.isArray(value)) {
        return `[${value.map(formatJSValue).join(", ")}]`;
    }
    if (typeof value === "object" && value !== null) {
        if ("tag" in value && "value" in value && Object.keys(value).length === 2) {
            return `#${value.tag} ${formatJSValue(value.value)}`;
        }
        if (value.type === "UUID") {
            return `UUID(${value.value})`;
        }
        if (value.type === "Point") {
            return `Point(${value.x}, ${value.y})`;
        }
        try {
            return JSON.stringify(value);
        } catch {
            return "[Object]";
        }
    }
    if (typeof value === "bigint") {
        return `${value}n`;
    }
    return JSON.stringify(value);
}

function getJSType(value) {
    if (value === null) return "null";
    if (value === undefined) return "undefined";
    if (Array.isArray(value)) return "Array";
    if (value instanceof Map) return "Map";
    if (value instanceof Set) return "Set";
    if (value instanceof Date) return "Date";
    if (typeof value === "bigint") return "BigInt";
    if (typeof value === "symbol") return "Symbol";
    if (typeof value === "object" && value.type) return value.type;
    return typeof value;
}

