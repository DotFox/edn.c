#include <stdlib.h>
#include <string.h>

#include "edn_internal.h"

/* Windows-specific intrinsics */
#if defined(_MSC_VER)
#include <intrin.h>

/* MSVC doesn't have __builtin_ctz, provide wrapper for _BitScanForward */
static inline int msvc_ctz(unsigned int mask) {
    unsigned long index;
    if (_BitScanForward(&index, mask)) {
        return (int) index;
    }
    return 32; /* No bit set */
}
#define CTZ(x) msvc_ctz(x)
#else
#define CTZ(x) __builtin_ctz(x)
#endif

#define INITIAL_CAPACITY 64

#define GROWTH_FACTOR 2

static bool newline_positions_grow(newline_positions_t* positions) {
    size_t new_capacity = positions->capacity * GROWTH_FACTOR;
    if (new_capacity < positions->capacity) {
        /* Overflow check */
        return false;
    }

    size_t* new_offsets =
        (size_t*) edn_arena_alloc(positions->arena, new_capacity * sizeof(size_t));
    if (!new_offsets) {
        return false;
    }

    /* Copy old data to new array */
    memcpy(new_offsets, positions->offsets, positions->count * sizeof(size_t));

    positions->offsets = new_offsets;
    positions->capacity = new_capacity;
    return true;
}

static bool newline_positions_add(newline_positions_t* positions, size_t offset) {
    if (positions->count >= positions->capacity) {
        if (!newline_positions_grow(positions)) {
            return false;
        }
    }

    positions->offsets[positions->count++] = offset;
    return true;
}

#if defined(__wasm__) && defined(__wasm_simd128__)

#include <wasm_simd128.h>

static bool newline_find_all_simd(newline_positions_t* positions, const char* data, size_t length) {
    const char* ptr = data;
    const char* end = data + length;

    while (ptr + 16 <= end) {
        v128_t chunk = wasm_v128_load((const v128_t*) ptr);
        v128_t newline = wasm_i8x16_eq(chunk, wasm_i8x16_splat('\n'));

        int mask = wasm_i8x16_bitmask(newline);

        while (mask != 0) {
            int bit_pos = CTZ((unsigned int) mask);
            size_t offset = (ptr - data) + bit_pos;

            if (!newline_positions_add(positions, offset)) {
                return false;
            }

            mask &= mask - 1;
        }

        ptr += 16;
    }

    /* Scalar tail: Process remaining bytes (0-15) */
    while (ptr < end) {
        if (*ptr == '\n') {
            size_t offset = ptr - data;
            if (!newline_positions_add(positions, offset)) {
                return false;
            }
        }
        ptr++;
    }

    return true;
}

#elif defined(__aarch64__) || defined(_M_ARM64)

#include <arm_neon.h>

static inline uint16_t neon_movemask_u8(uint8x16_t input) {
    static const uint8x16_t bitmask = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};

    uint8x16_t tmp = vandq_u8(input, bitmask);

    uint8x8_t lo = vget_low_u8(tmp);
    uint8x8_t hi = vget_high_u8(tmp);

    uint16_t lo_mask = (uint16_t) vaddv_u8(lo);
    uint16_t hi_mask = (uint16_t) vaddv_u8(hi);

    return (uint16_t) (lo_mask | (hi_mask << 8));
}

static bool newline_find_all_simd(newline_positions_t* positions, const char* data, size_t length) {
    const char* ptr = data;
    const char* end = data + length;

    while (ptr + 16 <= end) {
        uint8x16_t chunk = vld1q_u8((const uint8_t*) ptr);
        uint8x16_t newline = vceqq_u8(chunk, vdupq_n_u8('\n'));

        uint16_t mask = neon_movemask_u8(newline);

        while (mask != 0) {
            int bit_pos = CTZ((unsigned int) mask);
            size_t offset = (ptr - data) + bit_pos;

            if (!newline_positions_add(positions, offset)) {
                return false;
            }

            mask &= mask - 1;
        }

        ptr += 16;
    }

    /* Scalar tail: Process remaining bytes (0-15) */
    while (ptr < end) {
        if (*ptr == '\n') {
            size_t offset = ptr - data;
            if (!newline_positions_add(positions, offset)) {
                return false;
            }
        }
        ptr++;
    }

    return true;
}

#elif defined(__x86_64__) || defined(_M_X64)

#if defined(_MSC_VER)
#include <intrin.h> /* MSVC: includes all SSE/AVX intrinsics */
#else
#include <emmintrin.h> /* GCC/Clang: SSE2 (_mm_loadu_si128, _mm_set1_epi8) */
#include <smmintrin.h> /* GCC/Clang: SSE4.1 (_mm_cmpeq_epi8, etc.) */
#endif

static bool newline_find_all_simd(newline_positions_t* positions, const char* data, size_t length) {
    const char* ptr = data;
    const char* end = data + length;

    while (ptr + 16 <= end) {
        __m128i chunk = _mm_loadu_si128((const __m128i*) ptr);
        __m128i newline = _mm_cmpeq_epi8(chunk, _mm_set1_epi8('\n'));

        int mask = _mm_movemask_epi8(newline);

        while (mask != 0) {
            int bit_pos = CTZ((unsigned int) mask);
            size_t offset = (ptr - data) + bit_pos;

            if (!newline_positions_add(positions, offset)) {
                return false;
            }

            mask &= mask - 1;
        }

        ptr += 16;
    }

    /* Scalar tail: Process remaining bytes (0-15) */
    while (ptr < end) {
        if (*ptr == '\n') {
            size_t offset = ptr - data;
            if (!newline_positions_add(positions, offset)) {
                return false;
            }
        }
        ptr++;
    }

    return true;
}

#else
/* ====================================================================
 * Scalar Fallback Implementation
 * ==================================================================== */

static bool newline_find_all_simd(newline_positions_t* positions, const char* data, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (data[i] == '\n') {
            if (!newline_positions_add(positions, i)) {
                return false;
            }
        }
    }

    return true;
}

#endif

/* ========================================================================
 * PUBLIC API IMPLEMENTATION
 * ======================================================================== */

newline_positions_t* newline_positions_create(size_t initial_capacity, edn_arena_t* arena) {
    if (initial_capacity == 0) {
        initial_capacity = INITIAL_CAPACITY;
    }

    if (!arena) {
        return NULL;
    }

    newline_positions_t* positions =
        (newline_positions_t*) edn_arena_alloc(arena, sizeof(newline_positions_t));
    if (!positions) {
        return NULL;
    }

    positions->offsets = (size_t*) edn_arena_alloc(arena, initial_capacity * sizeof(size_t));
    if (!positions->offsets) {
        return NULL;
    }

    positions->count = 0;
    positions->capacity = initial_capacity;
    positions->arena = arena;

    return positions;
}

newline_positions_t* newline_find_all(const char* data, size_t length, edn_arena_t* arena) {
    if (!data) {
        return NULL;
    }

    newline_positions_t* positions = newline_positions_create(INITIAL_CAPACITY, arena);
    if (!positions) {
        return NULL;
    }

    if (!newline_find_all_simd(positions, data, length)) {
        return NULL;
    }

    return positions;
}

/* ========================================================================
 * EXTENDED LINE TERMINATOR DETECTION
 * ======================================================================== */

/**
 * Check if byte sequence is Unicode NEL (Next Line: 0xC2 0x85)
 */
static inline bool is_nel(const char* ptr, const char* end) {
    return (ptr + 1 < end) && ((unsigned char) ptr[0] == 0xC2) && ((unsigned char) ptr[1] == 0x85);
}

/**
 * Check if byte sequence is Unicode LS (Line Separator: 0xE2 0x80 0xA8)
 */
static inline bool is_ls(const char* ptr, const char* end) {
    return (ptr + 2 < end) && ((unsigned char) ptr[0] == 0xE2) &&
           ((unsigned char) ptr[1] == 0x80) && ((unsigned char) ptr[2] == 0xA8);
}

/**
 * Check if byte sequence is Unicode PS (Paragraph Separator: 0xE2 0x80 0xA9)
 */
static inline bool is_ps(const char* ptr, const char* end) {
    return (ptr + 2 < end) && ((unsigned char) ptr[0] == 0xE2) &&
           ((unsigned char) ptr[1] == 0x80) && ((unsigned char) ptr[2] == 0xA9);
}

/**
 * Find all line terminators with configurable mode (no SIMD optimization yet)
 */
static bool newline_find_all_ex_impl(newline_positions_t* positions, const char* data,
                                     size_t length, newline_mode_t mode) {
    const char* ptr = data;
    const char* end = data + length;

    switch (mode) {
        case NEWLINE_MODE_LF:
            /* Fast path: LF only (use existing SIMD implementation) */
            return newline_find_all_simd(positions, data, length);

        case NEWLINE_MODE_CRLF_AWARE:
            /* CRLF-aware: treat \r\n as one terminator at \n position */
            while (ptr < end) {
                unsigned char c = (unsigned char) *ptr;

                if (c == '\n') {
                    /* Found LF - record it */
                    if (!newline_positions_add(positions, ptr - data)) {
                        return false;
                    }
                    ptr++;
                } else if (c == '\r' && ptr + 1 < end && ptr[1] == '\n') {
                    /* Found CRLF - skip CR, record LF position on next iteration */
                    ptr++;
                } else {
                    ptr++;
                }
            }
            return true;

        case NEWLINE_MODE_ANY_ASCII:
            /* Any ASCII: recognize both \n and \r as separate terminators */
            while (ptr < end) {
                unsigned char c = (unsigned char) *ptr;

                if (c == '\n' || c == '\r') {
                    if (!newline_positions_add(positions, ptr - data)) {
                        return false;
                    }
                }
                ptr++;
            }
            return true;

        case NEWLINE_MODE_UNICODE:
            /* Unicode mode: recognize all Unicode line terminators */
            while (ptr < end) {
                unsigned char c = (unsigned char) *ptr;
                size_t bytes_to_skip = 1;
                bool is_terminator = false;
                size_t terminator_pos = ptr - data;

                if (c == '\n') {
                    /* LF */
                    is_terminator = true;
                } else if (c == '\r') {
                    /* CR - only if not followed by LF */
                    if (ptr + 1 < end && ptr[1] == '\n') {
                        /* CRLF - skip CR, will record LF next iteration */
                        ptr++;
                        continue;
                    }
                    is_terminator = true;
                } else if (is_nel(ptr, end)) {
                    /* NEL (0xC2 0x85) - record at first byte */
                    is_terminator = true;
                    bytes_to_skip = 2;
                } else if (is_ls(ptr, end)) {
                    /* LS (0xE2 0x80 0xA8) - record at first byte */
                    is_terminator = true;
                    bytes_to_skip = 3;
                } else if (is_ps(ptr, end)) {
                    /* PS (0xE2 0x80 0xA9) - record at first byte */
                    is_terminator = true;
                    bytes_to_skip = 3;
                }

                if (is_terminator) {
                    if (!newline_positions_add(positions, terminator_pos)) {
                        return false;
                    }
                }

                ptr += bytes_to_skip;
            }
            return true;

        default:
            /* Unknown mode - fall back to LF only */
            return newline_find_all_simd(positions, data, length);
    }
}

newline_positions_t* newline_find_all_ex(const char* data, size_t length, newline_mode_t mode,
                                         edn_arena_t* arena) {
    if (!data) {
        return NULL;
    }

    newline_positions_t* positions = newline_positions_create(INITIAL_CAPACITY, arena);
    if (!positions) {
        return NULL;
    }

    if (!newline_find_all_ex_impl(positions, data, length, mode)) {
        return NULL;
    }

    return positions;
}

/* ========================================================================
 * DOCUMENT POSITION FUNCTIONS
 * ======================================================================== */

/**
 * Binary search to find the line number for a given byte offset.
 *
 * Returns the index of the newline that ends the line containing the offset.
 * If offset is before the first newline, returns SIZE_MAX.
 */
static size_t binary_search_line(const newline_positions_t* positions, size_t byte_offset) {
    if (positions->count == 0 || byte_offset <= positions->offsets[0]) {
        /* Offset is on the first line (before or at first newline) */
        return SIZE_MAX;
    }

    /* Binary search for the last newline before byte_offset */
    size_t left = 0;
    size_t right = positions->count - 1;
    size_t result = SIZE_MAX;

    while (left <= right) {
        size_t mid = left + (right - left) / 2;

        if (positions->offsets[mid] < byte_offset) {
            /* This newline is before the offset, might be our answer */
            result = mid;
            left = mid + 1;
        } else {
            /* This newline is at or after the offset */
            if (mid == 0) {
                break;
            }
            right = mid - 1;
        }
    }

    return result;
}

bool newline_get_position(const newline_positions_t* positions, size_t byte_offset,
                          document_position_t* out_position) {
    if (!positions || !out_position) {
        return false;
    }

    /* Initialize output */
    out_position->byte_offset = byte_offset;

    /* Find which line this offset belongs to */
    size_t line_idx = binary_search_line(positions, byte_offset);

    if (line_idx == SIZE_MAX) {
        /* Offset is on the first line (before the first newline) */
        out_position->line = 1;
        out_position->column = byte_offset + 1; /* 1-indexed */
    } else {
        /* Offset is after newline at line_idx */
        out_position->line = line_idx + 2; /* +1 for 1-indexing, +1 for line after newline */

        /* Column is offset from the newline position */
        size_t line_start = positions->offsets[line_idx] + 1; /* After the newline */
        out_position->column = byte_offset - line_start + 1;  /* 1-indexed */
    }

    return true;
}
