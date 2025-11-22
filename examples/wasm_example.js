// Example usage of EDN.C WebAssembly bindings

const Module = require('../edn.js');

Module.onRuntimeInitialized = () => {
    console.log('EDN.C WebAssembly module loaded');
    console.log('SIMD128 support enabled\n');

    // Example EDN strings to parse
    const examples = [
        // Primitives
        { edn: 'nil', desc: 'Nil value' },
        { edn: 'true', desc: 'Boolean' },
        { edn: '42', desc: 'Integer' },
        { edn: '9007199254740992', desc: 'Large integer (BigInt)' },
        { edn: '3.14159', desc: 'Float' },
        { edn: '22/7', desc: 'Ratio', skip: !Module._wasm_edn_parse_to_js }, // Only if RATIO enabled
        { edn: '\\a', desc: 'Character' },
        { edn: '"hello world"', desc: 'String' },
        
        { edn: "1234_5678_9012_3456", desc: "Long credit card number" },
        { edn: "3.14_15", desc: "Pi" },
        { edn: "0xFF_EC_DE_5E", desc: "HEX bytes" },

        // Symbols and keywords
        { edn: 'my-symbol', desc: 'Symbol' },
        { edn: 'ns/qualified-symbol', desc: 'Namespaced symbol' },
        { edn: ':keyword', desc: 'Keyword' },
        { edn: ':ns/qualified-keyword', desc: 'Namespaced keyword' },
        
        // Collections
        { edn: '[1 2 3]', desc: 'Vector' },
        { edn: '(1 2 3)', desc: 'List' },
        { edn: '#{1 2 3}', desc: 'Set' },
        { edn: '{:name "Alice" :age 30}', desc: 'Map' },
        
        // Nested structures
        { edn: '[1 [2 [3 [4]]]]', desc: 'Nested vectors' },
        { edn: '{:user {:name "Bob" :scores [95 87 92]}}', desc: 'Nested map' },
        
        // Tagged literals
        { edn: '#inst "2024-01-15"', desc: 'Tagged literal (inst)' },
        { edn: '#uuid "550e8400-e29b-41d4-a716-446655440000"', desc: 'Tagged literal (uuid)' },
        { edn: '#myapp/custom {:data 42}', desc: 'Custom tagged literal' },
    ];

    console.log('='.repeat(70));
    console.log('EDN to JavaScript Conversion Examples');
    console.log('='.repeat(70));

    for (const example of examples) {
        if (example.skip) continue;

        console.log(`\n${example.desc}:`);
        console.log(`  EDN:  ${example.edn}`);

        try {
            // Parse EDN and convert to JavaScript
            const jsValue = Module.ccall(
                'wasm_edn_parse_to_js',
                'number', // Returns EM_VAL handle
                ['string'],
                [example.edn]
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

    console.log('\n' + '='.repeat(70));
    console.log('Advanced Examples');
    console.log('='.repeat(70));

    // Example: Working with Maps
    console.log('\n📦 Map Example:');
    const mapEDN = '{:name "Charlie" :age 25 :hobbies ["reading" "coding"]}';
    const mapHandle = Module.ccall('wasm_edn_parse_to_js', 'number', ['string'], [mapEDN]);
    if (mapHandle) {
        const map = Module.Emval.toValue(mapHandle);
        console.log(`  Original: ${mapEDN}`);
        console.log(`  JS Map size: ${map.size}`);
        console.log(`  Keys:`, Array.from(map.keys()).map(formatJSValue));
        console.log(`  Name: ${formatJSValue(map.get(Symbol.for(':name')))}`);
    }

    // Example: Working with Sets
    console.log('\n📦 Set Example:');
    const setEDN = '#{:red :green :blue}';
    const setHandle = Module.ccall('wasm_edn_parse_to_js', 'number', ['string'], [setEDN]);
    if (setHandle) {
        const set = Module.Emval.toValue(setHandle);
        console.log(`  Original: ${setEDN}`);
        console.log(`  JS Set size: ${set.size}`);
        console.log(`  Values:`, Array.from(set).map(formatJSValue));
    }

    // Example: BigInt handling
    console.log('\n📦 BigInt Example:');
    const bigintEDN = '999999999999999999999999999999';
    const bigintHandle = Module.ccall('wasm_edn_parse_to_js', 'number', ['string'], [bigintEDN]);
    if (bigintHandle) {
        const bigint = Module.Emval.toValue(bigintHandle);
        console.log(`  Original: ${bigintEDN}`);
        console.log(`  JS Value: ${bigint}`);
        console.log(`  Type: ${typeof bigint}`);
    }

    console.log('\n✅ All examples completed!\n');
};

// Helper functions
function formatJSValue(value) {
    if (value === null) return 'null';
    if (typeof value === 'symbol') return value.toString();
    if (value instanceof Map) {
        const entries = Array.from(value.entries())
            .map(([k, v]) => `${formatJSValue(k)}: ${formatJSValue(v)}`)
            .join(', ');
        return `Map { ${entries} }`;
    }
    if (value instanceof Set) {
        const values = Array.from(value).map(formatJSValue).join(', ');
        return `Set { ${values} }`;
    }
    if (Array.isArray(value)) {
        return `[${value.map(formatJSValue).join(', ')}]`;
    }
    if (typeof value === 'object' && value !== null && 'tag' in value && 'value' in value) {
        return `#${value.tag} ${formatJSValue(value.value)}`;
    }
    if (typeof value === 'object') {
        return JSON.stringify(value);
    }
    if (typeof value === 'bigint') {
        return `${value}n`;
    }
    return JSON.stringify(value);
}

function getJSType(value) {
    if (value === null) return 'null';
    if (Array.isArray(value)) return 'Array';
    if (value instanceof Map) return 'Map';
    if (value instanceof Set) return 'Set';
    if (typeof value === 'bigint') return 'BigInt';
    if (typeof value === 'symbol') return 'Symbol';
    return typeof value;
}
