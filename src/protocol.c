#include "ws/protocol.h"

#include "ws/format.h"

#include <stddef.h>

static char fold(char c)
{
    if (c >= 'a' && c <= 'z') {
        return (char)(c - ('a' - 'A'));
    }
    return c;
}

static const char *skip_space(const char *s)
{
    while (*s == ' ') {
        s++;
    }
    return s;
}

static bool take_word(const char **cursor, char *out, size_t cap)
{
    const char *s = skip_space(*cursor);
    size_t n = 0;

    if (*s == '\0') {
        return false;
    }
    while (*s != '\0' && *s != ' ') {
        if (n + 1u < cap) {
            out[n++] = fold(*s);
        }
        s++;
    }
    out[n] = '\0';
    *cursor = s;
    return true;
}

static bool word_is(const char *got, const char *want)
{
    while (*want != '\0') {
        if (*got != *want) {
            return false;
        }
        got++;
        want++;
    }
    return *got == '\0';
}

static bool parse_i32(const char *s, int32_t *out)
{
    bool neg = false;
    int32_t value = 0;
    bool any = false;

    s = skip_space(s);
    if (*s == '-') {
        neg = true;
        s++;
    } else if (*s == '+') {
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        int digit = *s - '0';
        if (value > (2147483647 - digit) / 10) {
            return false;
        }
        value = value * 10 + digit;
        any = true;
        s++;
    }
    s = skip_space(s);
    if (!any || *s != '\0') {
        return false;
    }
    *out = neg ? -value : value;
    return true;
}

static ws_command_t cmd(ws_cmd_id_t id, int32_t arg)
{
    ws_command_t out;

    out.id = id;
    out.arg = arg;
    return out;
}

ws_command_t ws_parse_command(const char *line)
{
    char word[16];
    const char *cursor = line;

    if (line == NULL || !take_word(&cursor, word, sizeof word)) {
        return cmd(WS_CMD_NONE, 0);
    }
    if (word_is(word, "HELP") || word_is(word, "?")) {
        return *skip_space(cursor) == '\0' ? cmd(WS_CMD_HELP, 0) : cmd(WS_CMD_SYNTAX, 0);
    }
    if (word_is(word, "VER")) {
        return *skip_space(cursor) == '\0' ? cmd(WS_CMD_VER, 0) : cmd(WS_CMD_SYNTAX, 0);
    }
    if (word_is(word, "READ")) {
        return *skip_space(cursor) == '\0' ? cmd(WS_CMD_READ, 0) : cmd(WS_CMD_SYNTAX, 0);
    }
    if (word_is(word, "STATUS")) {
        return *skip_space(cursor) == '\0' ? cmd(WS_CMD_STATUS, 0) : cmd(WS_CMD_SYNTAX, 0);
    }
    if (word_is(word, "CFG")) {
        return *skip_space(cursor) == '\0' ? cmd(WS_CMD_CFG, 0) : cmd(WS_CMD_SYNTAX, 0);
    }
    if (word_is(word, "SAVE")) {
        return *skip_space(cursor) == '\0' ? cmd(WS_CMD_SAVE, 0) : cmd(WS_CMD_SYNTAX, 0);
    }
    if (word_is(word, "LOAD")) {
        return *skip_space(cursor) == '\0' ? cmd(WS_CMD_LOAD, 0) : cmd(WS_CMD_SYNTAX, 0);
    }
    if (word_is(word, "DEFAULTS")) {
        return *skip_space(cursor) == '\0' ? cmd(WS_CMD_DEFAULTS, 0) : cmd(WS_CMD_SYNTAX, 0);
    }
    if (word_is(word, "PAGE")) {
        return *skip_space(cursor) == '\0' ? cmd(WS_CMD_PAGE, 0) : cmd(WS_CMD_SYNTAX, 0);
    }
    if (word_is(word, "HOLD")) {
        return *skip_space(cursor) == '\0' ? cmd(WS_CMD_HOLD, 0) : cmd(WS_CMD_SYNTAX, 0);
    }
    if (word_is(word, "RESET")) {
        return *skip_space(cursor) == '\0' ? cmd(WS_CMD_RESET, 0) : cmd(WS_CMD_SYNTAX, 0);
    }
    if (word_is(word, "SET")) {
        char which[16];
        char arg[16];
        int32_t number = 0;

        if (!take_word(&cursor, which, sizeof which)) {
            return cmd(WS_CMD_SYNTAX, 0);
        }
        if (word_is(which, "UNIT")) {
            if (!take_word(&cursor, arg, sizeof arg) || *skip_space(cursor) != '\0') {
                return cmd(WS_CMD_SYNTAX, 0);
            }
            if (word_is(arg, "M")) {
                return cmd(WS_CMD_SET_UNIT, 0);
            }
            if (word_is(arg, "I")) {
                return cmd(WS_CMD_SET_UNIT, 1);
            }
            return cmd(WS_CMD_SYNTAX, 0);
        }
        if (word_is(which, "ALT")) {
            if (!parse_i32(cursor, &number)) {
                return cmd(WS_CMD_SYNTAX, 0);
            }
            return cmd(WS_CMD_SET_ALT, number);
        }
        if (word_is(which, "PERIOD")) {
            if (!parse_i32(cursor, &number)) {
                return cmd(WS_CMD_SYNTAX, 0);
            }
            return cmd(WS_CMD_SET_PERIOD, number);
        }
        if (word_is(which, "TELEM")) {
            if (!take_word(&cursor, arg, sizeof arg) || *skip_space(cursor) != '\0') {
                return cmd(WS_CMD_SYNTAX, 0);
            }
            if (word_is(arg, "ON")) {
                return cmd(WS_CMD_SET_TELEM, 1);
            }
            if (word_is(arg, "OFF")) {
                return cmd(WS_CMD_SET_TELEM, 0);
            }
            return cmd(WS_CMD_SYNTAX, 0);
        }
        return cmd(WS_CMD_SYNTAX, 0);
    }
    return cmd(WS_CMD_UNKNOWN, 0);
}

size_t ws_format_telemetry(char *dst, size_t cap, const ws_telem_t *in)
{
    size_t n = 0;

    n = ws_fmt_str(dst, cap, n, "WS t=");
    n = ws_fmt_i32_at(dst, cap, n, in->temp_c_x100);
    n = ws_fmt_str(dst, cap, n, " h=");
    n = ws_fmt_u32_at(dst, cap, n, in->rh_x100);
    n = ws_fmt_str(dst, cap, n, " p=");
    n = ws_fmt_u32_at(dst, cap, n, in->pressure_pa);
    n = ws_fmt_str(dst, cap, n, " sl=");
    n = ws_fmt_u32_at(dst, cap, n, in->sea_level_pa);
    n = ws_fmt_str(dst, cap, n, " td=");
    n = ws_fmt_i32_at(dst, cap, n, in->dew_c_x100);
    n = ws_fmt_str(dst, cap, n, " hi=");
    if (in->heat_applicable) {
        n = ws_fmt_i32_at(dst, cap, n, in->heat_c_x100);
    } else {
        n = ws_fmt_str(dst, cap, n, "na");
    }
    n = ws_fmt_str(dst, cap, n, " tr=");
    n = ws_fmt_i32_at(dst, cap, n, in->tendency);
    n = ws_fmt_str(dst, cap, n, " f=");
    n = ws_fmt_hex16(dst, cap, n, in->faults);
    n = ws_fmt_str(dst, cap, n, " u=");
    n = ws_fmt_u32_at(dst, cap, n, in->units);
    n = ws_fmt_str(dst, cap, n, "\n");
    return n;
}
