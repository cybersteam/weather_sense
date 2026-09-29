#pragma once

#include <stddef.h>
#include <stdint.h>

/*
 * snprintf-style helpers: they always NUL-terminate when cap > 0 and return
 * the length that would have been written, excluding the NUL. No printf, so
 * the AVR image does not pull in the float or integer printf libraries.
 */
size_t ws_fmt_i32(char *dst, size_t cap, int32_t value);
size_t ws_fmt_u32(char *dst, size_t cap, uint32_t value);
size_t ws_fmt_str(char *dst, size_t cap, size_t at, const char *text);
size_t ws_fmt_i32_at(char *dst, size_t cap, size_t at, int32_t value);
size_t ws_fmt_u32_at(char *dst, size_t cap, size_t at, uint32_t value);
size_t ws_fmt_hex16(char *dst, size_t cap, size_t at, uint16_t value);

/*
 * Print a scaled integer. `scale_digits` is how many decimal digits `value`
 * carries; `decimals` is how many to show after rounding half away from zero.
 * ws_fmt_fixed(buf, n, 2508, 2, 1) writes "25.1".
 */
size_t ws_fmt_fixed(char *dst, size_t cap, int32_t value,
                    unsigned scale_digits, unsigned decimals);
