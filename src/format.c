#include "ws/format.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static size_t finish(char *dst, size_t cap, const char *tmp, size_t n, bool reversed)
{
    size_t w;
    size_t i;

    if (cap == 0) {
        return n;
    }
    w = (n < cap - 1u) ? n : cap - 1u;
    for (i = 0; i < w; i++) {
        dst[i] = reversed ? tmp[n - 1u - i] : tmp[i];
    }
    dst[w] = '\0';
    return n;
}

size_t ws_fmt_u32(char *dst, size_t cap, uint32_t value)
{
    char tmp[10];
    size_t n = 0;

    do {
        tmp[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value > 0u && n < sizeof tmp);
    return finish(dst, cap, tmp, n, true);
}

size_t ws_fmt_i32(char *dst, size_t cap, int32_t value)
{
    char body[12];
    char out[12];
    size_t n = 0;
    size_t i;
    uint32_t mag;

    if (value < 0) {
        if (value == (int32_t)0x80000000) {
            return finish(dst, cap, "-2147483648", 11, false);
        }
        out[n++] = '-';
        mag = (uint32_t)(-value);
    } else {
        mag = (uint32_t)value;
    }
    ws_fmt_u32(body, sizeof body, mag);
    for (i = 0; body[i] != '\0' && n < sizeof out; i++) {
        out[n++] = body[i];
    }
    return finish(dst, cap, out, n, false);
}

size_t ws_fmt_str(char *dst, size_t cap, size_t at, const char *text)
{
    size_t n = at;

    if (text == NULL) {
        text = "";
    }
    while (*text != '\0') {
        if (cap > 0u && n + 1u < cap) {
            dst[n] = *text;
        }
        text++;
        n++;
    }
    if (cap > 0u) {
        size_t end = (n < cap) ? n : cap - 1u;
        dst[end] = '\0';
    }
    return n;
}

size_t ws_fmt_i32_at(char *dst, size_t cap, size_t at, int32_t value)
{
    char body[16];

    ws_fmt_i32(body, sizeof body, value);
    return ws_fmt_str(dst, cap, at, body);
}

size_t ws_fmt_u32_at(char *dst, size_t cap, size_t at, uint32_t value)
{
    char body[16];

    ws_fmt_u32(body, sizeof body, value);
    return ws_fmt_str(dst, cap, at, body);
}

size_t ws_fmt_hex16(char *dst, size_t cap, size_t at, uint16_t value)
{
    char body[5];
    int i;
    static const char hex[] = "0123456789ABCDEF";

    for (i = 3; i >= 0; i--) {
        body[i] = hex[value & 0x0Fu];
        value = (uint16_t)(value >> 4);
    }
    body[4] = '\0';
    return ws_fmt_str(dst, cap, at, body);
}

size_t ws_fmt_fixed(char *dst, size_t cap, int32_t value,
                    unsigned scale_digits, unsigned decimals)
{
    char out[20];
    size_t n = 0;
    unsigned i;
    int32_t div = 1;
    int32_t frac_scale = 1;
    int32_t magnitude;
    int32_t whole;
    int32_t frac;
    bool neg;

    if (decimals > scale_digits || scale_digits > 9u) {
        return ws_fmt_str(dst, cap, 0, "0");
    }
    for (i = 0; i < scale_digits - decimals; i++) {
        div *= 10;
    }
    for (i = 0; i < decimals; i++) {
        frac_scale *= 10;
    }

    if (value == (int32_t)0x80000000) {
        value += 1;
    }
    neg = value < 0;
    magnitude = neg ? -value : value;
    magnitude = (magnitude + div / 2) / div;
    if (decimals == 0u) {
        whole = magnitude;
        frac = 0;
    } else {
        whole = magnitude / frac_scale;
        frac = magnitude % frac_scale;
    }
    if (neg && (whole != 0 || frac != 0)) {
        out[n++] = '-';
    }

    {
        char digits[12];
        size_t dn = ws_fmt_u32(digits, sizeof digits, (uint32_t)whole);
        for (i = 0; i < dn && n < sizeof out; i++) {
            out[n++] = digits[i];
        }
    }
    if (decimals > 0u && n < sizeof out) {
        out[n++] = '.';
        {
            char digits[12];
            size_t dn = ws_fmt_u32(digits, sizeof digits, (uint32_t)frac);
            unsigned pad = decimals;
            if (dn < decimals) {
                pad = decimals - (unsigned)dn;
            } else {
                pad = 0;
            }
            while (pad > 0u && n < sizeof out) {
                out[n++] = '0';
                pad--;
            }
            for (i = 0; i < dn && n < sizeof out; i++) {
                out[n++] = digits[i];
            }
        }
    }
    return finish(dst, cap, out, n, false);
}
