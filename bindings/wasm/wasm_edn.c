/**
 * EDN.C - WebAssembly Bindings
 *
 * Exposes EDN.C functions to JavaScript through WebAssembly with SIMD128 support.
 * Provides automatic conversion from EDN values to JavaScript objects.
 */

#include <emscripten/emscripten.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/edn.h"

// External Emval functions
extern void _emval_decref(int handle);

// Forward declaration for recursive conversion
static int edn_to_js_internal(const edn_value_t* value);

/**
 * Parse EDN string and return result pointer.
 * JavaScript can check if pointer is NULL to detect errors.
 *
 * @param input EDN string (null-terminated)
 * @return Pointer to edn_value_t or NULL on error
 */
EMSCRIPTEN_KEEPALIVE
edn_value_t* wasm_edn_parse(const char* input) {
    if (!input) {
        return NULL;
    }

    edn_result_t result = edn_read(input, 0);

    if (result.error != EDN_OK) {
        // Error occurred - JavaScript can check for NULL
        return NULL;
    }

    return result.value;
}

EMSCRIPTEN_KEEPALIVE
void wasm_edn_free(edn_value_t* value) {
    if (value) {
        edn_free(value);
    }
}

EMSCRIPTEN_KEEPALIVE
int wasm_edn_type(edn_value_t* value) {
    if (!value) {
        return -1;
    }
    return (int) edn_type(value);
}

EMSCRIPTEN_KEEPALIVE
int64_t wasm_edn_get_int(edn_value_t* value) {
    if (!value || edn_type(value) != EDN_TYPE_INT) {
        return 0;
    }

    int64_t result;
    edn_int64_get(value, &result);
    return result;
}

EMSCRIPTEN_KEEPALIVE
int wasm_edn_get_bool(edn_value_t* value) {
    if (!value || edn_type(value) != EDN_TYPE_BOOL) {
        return 0;
    }

    bool result;
    edn_bool_get(value, &result);
    return result ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_edn_get_string(edn_value_t* value) {
    if (!value || edn_type(value) != EDN_TYPE_STRING) {
        return NULL;
    }

    size_t len;
    return edn_string_get(value, &len);
}

EMSCRIPTEN_KEEPALIVE
size_t wasm_edn_get_string_length(edn_value_t* value) {
    if (!value || edn_type(value) != EDN_TYPE_STRING) {
        return 0;
    }

    size_t len;
    edn_string_get(value, &len);
    return len;
}

EMSCRIPTEN_KEEPALIVE
size_t wasm_edn_count(edn_value_t* value) {
    if (!value) {
        return 0;
    }

    edn_type_t type = edn_type(value);

    switch (type) {
        case EDN_TYPE_VECTOR:
            return edn_vector_count(value);
        case EDN_TYPE_LIST:
            return edn_list_count(value);
        case EDN_TYPE_MAP:
            return edn_map_count(value);
        case EDN_TYPE_SET:
            return edn_set_count(value);
        default:
            return 0;
    }
}

EMSCRIPTEN_KEEPALIVE
edn_value_t* wasm_edn_vector_get(edn_value_t* value, size_t index) {
    if (!value || edn_type(value) != EDN_TYPE_VECTOR) {
        return NULL;
    }

    if (index >= edn_vector_count(value)) {
        return NULL;
    }

    return edn_vector_get(value, index);
}

EMSCRIPTEN_KEEPALIVE
edn_value_t* wasm_edn_map_get_at(edn_value_t* value, size_t index, edn_value_t** out_key) {
    if (!value || edn_type(value) != EDN_TYPE_MAP) {
        return NULL;
    }

    if (index >= edn_map_count(value)) {
        return NULL;
    }

    // Get both key and value using separate API calls
    if (out_key) {
        *out_key = edn_map_get_key(value, index);
    }
    return edn_map_get_value(value, index);
}

EMSCRIPTEN_KEEPALIVE
edn_value_t* wasm_edn_map_get_key(edn_value_t* value, size_t index) {
    if (!value || edn_type(value) != EDN_TYPE_MAP) {
        return NULL;
    }

    if (index >= edn_map_count(value)) {
        return NULL;
    }

    return edn_map_get_key(value, index);
}

EMSCRIPTEN_KEEPALIVE
edn_value_t* wasm_edn_map_get_value(edn_value_t* value, size_t index) {
    if (!value || edn_type(value) != EDN_TYPE_MAP) {
        return NULL;
    }

    if (index >= edn_map_count(value)) {
        return NULL;
    }

    return edn_map_get_value(value, index);
}

EMSCRIPTEN_KEEPALIVE
int wasm_edn_validate(const char* input) {
    if (!input) {
        return 0;
    }

    edn_result_t result = edn_read(input, 0);

    if (result.error == EDN_OK) {
        edn_free(result.value);
        return 1;
    }

    return 0;
}

EMSCRIPTEN_KEEPALIVE
const char* wasm_edn_get_error(const char* input) {
    if (!input) {
        return "Input is NULL";
    }

    edn_result_t result = edn_read(input, 0);

    if (result.error != EDN_OK) {
        // Note: This string lifetime is static, safe to return
        return result.error_message;
    }

    edn_free(result.value);
    return NULL;
}

/**
 * Convert EDN value to JavaScript object.
 * Uses recursive conversion with the following type mappings:
 * - nil -> null
 * - boolean -> boolean
 * - integer -> number (or BigInt if out of safe integer range)
 * - bigint -> BigInt
 * - float -> number
 * - bigdec -> string (preserves precision)
 * - ratio -> number (computed as numerator/denominator)
 * - character -> string (single character)
 * - string -> string
 * - symbol -> Symbol
 * - keyword -> Symbol (includes ':' prefix)
 * - list -> Array
 * - vector -> Array
 * - map -> Map
 * - set -> Set
 * - tagged -> object with {tag, value} fields
 *
 * @return JavaScript value ID that must be cleaned up
 */
// clang-format off
static int edn_to_js_internal(const edn_value_t* value) {
    if (!value) {
        return EM_ASM_INT({ return Emval.toHandle(null); });
    }

    edn_type_t type = edn_type(value);

    switch (type) {
        case EDN_TYPE_NIL: {
            return EM_ASM_INT({ return Emval.toHandle(null); });
        }

        case EDN_TYPE_BOOL: {
            bool b;
            edn_bool_get(value, &b);
            return EM_ASM_INT({
                return Emval.toHandle($0 !== 0);
            }, b);
        }

        case EDN_TYPE_INT: {
            int64_t num;
            edn_int64_get(value, &num);

            // Check if number fits in JavaScript safe integer range
            if (num >= -9007199254740991LL && num <= 9007199254740991LL) {
                return EM_ASM_INT({ return Emval.toHandle(Number($0)); }, (double)num);
            } else {
                // Use BigInt for large integers (convert to string first)
                char buf[32];
                snprintf(buf, sizeof(buf), "%lld", (long long)num);
                return EM_ASM_INT({ return Emval.toHandle(BigInt(UTF8ToString($0))); }, buf);
            }
        }

        case EDN_TYPE_BIGINT: {
            size_t len;
            bool negative;
            uint8_t radix;
            const char* digits = edn_bigint_get(value, &len, &negative, &radix);

            return EM_ASM_INT({
                const digits = UTF8ToString($0, $1);
                const negative = $2 !== 0;
                const radix = $3;

                let bigint;
                if (radix === 10) {
                    bigint = BigInt(digits);
                } else if (radix === 16) {
                    bigint = BigInt('0x' + digits);
                } else if (radix === 8) {
                    bigint = BigInt('0o' + digits);
                } else if (radix === 2) {
                    bigint = BigInt('0b' + digits);
                } else {
                    bigint = BigInt(parseInt(digits, radix));
                }

                return Emval.toHandle(negative ? -bigint : bigint);
            }, digits, len, negative, radix);
        }

        case EDN_TYPE_FLOAT: {
            double d;
            edn_double_get(value, &d);
            return EM_ASM_INT({ return Emval.toHandle($0); }, d);
        }

        case EDN_TYPE_BIGDEC: {
            size_t len;
            bool negative;
            const char* dec_str = edn_bigdec_get(value, &len, &negative);

            // Return as string to preserve precision
            return EM_ASM_INT({
                const str = UTF8ToString($0);
                const negative = $1 !== 0;
                return Emval.toHandle(negative ? '-' + str : str);
            }, dec_str, negative);
        }

#ifdef EDN_ENABLE_RATIO
        case EDN_TYPE_RATIO: {
            int64_t numerator, denominator;
            edn_ratio_get(value, &numerator, &denominator);

            return EM_ASM_INT(
                { return Emval.toHandle($0 / $1); }, (double) numerator, (double) denominator);
        }

        case EDN_TYPE_BIGRATIO: {
            const char* numerator_digits;
            const char* denominator_digits;
            size_t numer_length;
            size_t denom_length;
            bool negative;
            edn_bigratio_get(value, &numerator_digits, &numer_length, &negative, &denominator_digits, &denom_length);

            return EM_ASM_INT({
                const numerator_str = UTF8ToString($0, $1);
                const denominator_str = UTF8ToString($2, $3);
                const negative = $4 !== 0;

                return Emval.toHandle((negative ? '-' + numerator_str : numerator_str) + '/' + denominator_str);
            },  numerator_digits, numer_length, denominator_digits, denom_length, negative);
        }
#endif

        case EDN_TYPE_CHARACTER: {
            uint32_t codepoint;
            edn_character_get(value, &codepoint);

            return EM_ASM_INT({ return Emval.toHandle(String.fromCodePoint($0)); }, codepoint);
        }

        case EDN_TYPE_STRING: {
            size_t len;
            const char* str = edn_string_get(value, &len);

            return EM_ASM_INT({ return Emval.toHandle(UTF8ToString($0, $1)); }, str, len);
        }

        case EDN_TYPE_SYMBOL: {
            const char* ns = NULL;
            const char* name = NULL;
            size_t ns_len = 0;
            size_t name_len = 0;

            edn_symbol_get(value, &ns, &ns_len, &name, &name_len);

            if (ns && ns_len > 0) {
                return EM_ASM_INT(
                    {
                        const ns = UTF8ToString($0, $1);
                        const name = UTF8ToString($2, $3);
                        return Emval.toHandle(Symbol.for (ns + '/' + name));
                    },
                    ns, ns_len, name, name_len);
            } else {
                return EM_ASM_INT(
                    {
                        const name = UTF8ToString($0, $1);
                        return Emval.toHandle(Symbol.for (name));
                    },
                    name, name_len);
            }
        }

        case EDN_TYPE_KEYWORD: {
            const char* ns = NULL;
            const char* name = NULL;
            size_t ns_len = 0;
            size_t name_len = 0;

            edn_keyword_get(value, &ns, &ns_len, &name, &name_len);

            if (ns && ns_len > 0) {
                return EM_ASM_INT(
                    {
                        const ns = UTF8ToString($0, $1);
                        const name = UTF8ToString($2, $3);
                        return Emval.toHandle(Symbol.for (':' + ns + '/' + name));
                    },
                    ns, ns_len, name, name_len);
            } else {
                return EM_ASM_INT(
                    {
                        const name = UTF8ToString($0, $1);
                        return Emval.toHandle(Symbol.for (':' + name));
                    },
                    name, name_len);
            }
        }

        case EDN_TYPE_LIST:
        case EDN_TYPE_VECTOR: {
            size_t count =
                (type == EDN_TYPE_LIST) ? edn_list_count(value) : edn_vector_count(value);

            int arr = EM_ASM_INT({ return Emval.toHandle([]); });

            for (size_t i = 0; i < count; i++) {
                edn_value_t* elem =
                    (type == EDN_TYPE_LIST) ? edn_list_get(value, i) : edn_vector_get(value, i);

                int js_elem = edn_to_js_internal(elem);

                EM_ASM(
                    {
                        const arr = Emval.toValue($0);
                        const elem = Emval.toValue($1);
                        arr.push(elem);
                    },
                    arr, js_elem);

                // Free the element handle
                _emval_decref(js_elem);
            }

            return arr;
        }

        case EDN_TYPE_SET: {
            size_t count = edn_set_count(value);

            int set = EM_ASM_INT({ return Emval.toHandle(new Set()); });

            for (size_t i = 0; i < count; i++) {
                edn_value_t* elem = edn_set_get(value, i);
                int js_elem = edn_to_js_internal(elem);

                EM_ASM(
                    {
                        const set = Emval.toValue($0);
                        const elem = Emval.toValue($1);
                        set.add(elem);
                    },
                    set, js_elem);

                // Free the element handle
                _emval_decref(js_elem);
            }

            return set;
        }

        case EDN_TYPE_MAP: {
            size_t count = edn_map_count(value);

            int map = EM_ASM_INT({ return Emval.toHandle(new Map()); });

            for (size_t i = 0; i < count; i++) {
                edn_value_t* key = edn_map_get_key(value, i);
                edn_value_t* val = edn_map_get_value(value, i);

                int js_key = edn_to_js_internal(key);
                int js_val = edn_to_js_internal(val);

                EM_ASM(
                    {
                        const map = Emval.toValue($0);
                        const key = Emval.toValue($1);
                        const val = Emval.toValue($2);
                        map.set(key, val);
                    },
                    map, js_key, js_val);

                // Free the key and value handles
                _emval_decref(js_key);
                _emval_decref(js_val);
            }

            return map;
        }

        case EDN_TYPE_TAGGED: {
            const char* tag = NULL;
            size_t tag_len = 0;
            edn_value_t* tagged_value = NULL;

            edn_tagged_get(value, &tag, &tag_len, &tagged_value);

            int js_value = edn_to_js_internal(tagged_value);

            int obj = EM_ASM_INT(
                {
                    const tag = UTF8ToString($0, $1);
                    const value = Emval.toValue($2);
                    return Emval.toHandle({tag : tag, value : value});
                },
                tag, tag_len, js_value);

            // Free the value handle
            _emval_decref(js_value);

            return obj;
        }

        default:
            return EM_ASM_INT({ return Emval.toHandle(null); });
    }
}
// clang-format on

/**
 * Convert EDN value to JavaScript object (exported function).
 *
 * @param value EDN value pointer (from wasm_edn_parse)
 * @return JavaScript value handle (must be cleaned up with _emval_decref)
 */
EMSCRIPTEN_KEEPALIVE
int wasm_edn_to_js(edn_value_t* value) {
    return edn_to_js_internal(value);
}

/**
 * Parse EDN string and return as JavaScript object.
 *
 * @param input EDN string (null-terminated)
 * @return JavaScript object handle or null on error (must be cleaned up with _emval_decref)
 */
EMSCRIPTEN_KEEPALIVE
int wasm_edn_parse_to_js(const char* input) {
    if (!input) {
        return EM_ASM_INT({ return Emval.toHandle(null); });
    }

    edn_result_t result = edn_read(input, 0);

    if (result.error != EDN_OK) {
        return EM_ASM_INT({ return Emval.toHandle(null); });
    }

    int js_value = edn_to_js_internal(result.value);
    edn_free(result.value);

    return js_value;
}
