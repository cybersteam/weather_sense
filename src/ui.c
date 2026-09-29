#include "ws/ui.h"

#include "ws/config_store.h"
#include "ws/faults.h"
#include "ws/format.h"
#include "ws/metrics.h"

static void blank(char line[17])
{
    unsigned i;

    for (i = 0; i < 16u; i++) {
        line[i] = ' ';
    }
    line[16] = '\0';
}

static void place(char line[17], int col, const char *text)
{
    if (text == NULL) {
        return;
    }
    while (*text != '\0' && col >= 0 && col < 16) {
        line[col++] = *text++;
    }
}

static void place_right(char line[17], const char *text)
{
    int n = 0;
    int col;

    while (text[n] != '\0') {
        n++;
    }
    col = 16 - n;
    if (col < 0) {
        col = 0;
    }
    place(line, col, text);
}

static void append_text(char *dst, size_t cap, const char *text)
{
    size_t n = 0;

    while (dst[n] != '\0') {
        n++;
    }
    (void)ws_fmt_str(dst, cap, n, text);
}

static void format_temp(char *dst, size_t cap, int32_t temp_c_x100, uint8_t units)
{
    int32_t shown = temp_c_x100;
    char unit = 'C';

    if (units == WS_UNIT_IMPERIAL) {
        shown = ws_c_to_f_x100(temp_c_x100);
        unit = 'F';
    }
    (void)ws_fmt_fixed(dst, cap, shown, 2u, 1u);
    append_text(dst, cap, unit == 'F' ? "F" : "C");
}

static void format_humidity(char *dst, size_t cap, uint16_t rh_x100)
{
    uint32_t pct = ((uint32_t)rh_x100 + 50u) / 100u;
    size_t n;

    if (pct > 100u) {
        pct = 100u;
    }
    n = ws_fmt_u32(dst, cap, pct);
    if (n + 1u < cap) {
        dst[n] = '%';
        dst[n + 1u] = '\0';
    }
}

static void format_pressure(char *dst, size_t cap, uint32_t pressure_pa, uint8_t units)
{
    if (units == WS_UNIT_IMPERIAL) {
        uint32_t inhg_x100 = (uint32_t)(((uint64_t)pressure_pa * 1000u + 16932u) / 33864u);
        (void)ws_fmt_fixed(dst, cap, (int32_t)inhg_x100, 2u, 2u);
        append_text(dst, cap, "inHg");
        return;
    }
    {
        uint32_t hpa_x10 = (pressure_pa + 5u) / 10u;
        (void)ws_fmt_fixed(dst, cap, (int32_t)hpa_x10, 1u, 1u);
        append_text(dst, cap, "hPa");
    }
}

static char tendency_mark(int8_t tendency)
{
    if (tendency == WS_TEND_RISING) {
        return '^';
    }
    if (tendency == WS_TEND_FALLING) {
        return 'v';
    }
    if (tendency == WS_TEND_STEADY) {
        return '-';
    }
    return '?';
}

static const char *fault_name(uint16_t faults)
{
    if ((faults & WS_FAULT_BME_ID) != 0u) {
        return "BME missing";
    }
    if ((faults & WS_FAULT_BME_TIMEOUT) != 0u) {
        return "BME timeout";
    }
    if ((faults & WS_FAULT_BME_RANGE) != 0u) {
        return "BME range";
    }
    if ((faults & WS_FAULT_BME_STALE) != 0u) {
        return "BME stale";
    }
    if ((faults & WS_FAULT_CONFIG) != 0u) {
        return "Config CRC";
    }
    if ((faults & WS_FAULT_WDT_RESET) != 0u) {
        return "WDT reset";
    }
    if ((faults & WS_FAULT_BROWNOUT) != 0u) {
        return "Brown-out";
    }
    return "System OK";
}

static const char *reset_name(ws_reset_reason_t reason)
{
    switch (reason) {
    case WS_RESET_POWER_ON:
        return "POR";
    case WS_RESET_EXTERNAL:
        return "EXT";
    case WS_RESET_BROWNOUT:
        return "BOR";
    case WS_RESET_WATCHDOG:
        return "WDT";
    default:
        return "UNK";
    }
}

static void format_uptime(char *dst, size_t cap, uint32_t seconds)
{
    size_t n = 0;

    if (seconds >= 86400u) {
        n = ws_fmt_u32(dst, cap, seconds / 86400u);
        n = ws_fmt_str(dst, cap, n, "d");
        n = ws_fmt_u32_at(dst, cap, n, (seconds % 86400u) / 3600u);
        (void)ws_fmt_str(dst, cap, n, "h");
        return;
    }
    if (seconds >= 3600u) {
        n = ws_fmt_u32(dst, cap, seconds / 3600u);
        n = ws_fmt_str(dst, cap, n, "h");
        {
            uint32_t minutes = (seconds % 3600u) / 60u;
            char body[4];
            (void)ws_fmt_u32(body, sizeof body, minutes);
            if (minutes < 10u) {
                n = ws_fmt_str(dst, cap, n, "0");
            }
            n = ws_fmt_str(dst, cap, n, body);
        }
        (void)ws_fmt_str(dst, cap, n, "m");
        return;
    }
    if (seconds >= 60u) {
        n = ws_fmt_u32(dst, cap, seconds / 60u);
        n = ws_fmt_str(dst, cap, n, "m");
        {
            uint32_t sec = seconds % 60u;
            char body[4];
            (void)ws_fmt_u32(body, sizeof body, sec);
            if (sec < 10u) {
                n = ws_fmt_str(dst, cap, n, "0");
            }
            n = ws_fmt_str(dst, cap, n, body);
        }
        (void)ws_fmt_str(dst, cap, n, "s");
        return;
    }
    n = ws_fmt_u32(dst, cap, seconds);
    (void)ws_fmt_str(dst, cap, n, "s");
}

static void format_altitude(char *dst, size_t cap, int16_t altitude_m, uint8_t units)
{
    int32_t shown = altitude_m;
    size_t n;

    if (units == WS_UNIT_IMPERIAL) {
        /* Nearest foot. 1 m = 3.28084 ft. */
        shown = (int32_t)(((int64_t)altitude_m * 328 + 50) / 100);
    }
    n = ws_fmt_i32(dst, cap, shown);
    if (units == WS_UNIT_IMPERIAL) {
        (void)ws_fmt_str(dst, cap, n, "ft");
    } else {
        (void)ws_fmt_str(dst, cap, n, "m");
    }
}

void ws_ui_format(const ws_ui_input_t *in, char line0[17], char line1[17])
{
    char field[16];

    blank(line0);
    blank(line1);
    if (in->editing) {
        format_altitude(field, sizeof field, in->altitude_m, in->units);
        place(line0, 0, "Alt");
        place_right(line0, field);
        place(line1, 0, "UP/DN SEL=save");
        return;
    }

    if (in->page == 3u || (!in->have_sample && in->faults != 0u && in->page == 0u)) {
        if (in->faults == 0u) {
            place(line0, 0, "System OK");
            if (in->hold) {
                place_right(line0, "HOLD");
            }
            place(line1, 0, "Reset");
            place_right(line1, reset_name(in->reset_reason));
            return;
        }
        place(line0, 0, "FAULT");
        (void)ws_fmt_hex16(field, sizeof field, 0, in->faults);
        place_right(line0, field);
        place(line1, 0, fault_name(in->faults));
        return;
    }

    if (!in->have_sample) {
        place(line0, 0, "WeatherSense");
        place(line1, 0, "waiting");
        return;
    }

    if (in->page == 1u) {
        format_temp(field, sizeof field, in->dew_c_x100, in->units);
        place(line0, 0, "Dew");
        place_right(line0, field);
        format_pressure(field, sizeof field, in->sea_level_pa, in->units);
        place(line1, 0, "MSL");
        place_right(line1, field);
        return;
    }

    if (in->page == 2u) {
        place(line0, 0, "Heat");
        if (!in->heat_applicable) {
            place_right(line0, "n/a");
        } else {
            format_temp(field, sizeof field, in->heat_c_x100, in->units);
            place_right(line0, field);
        }
        format_uptime(field, sizeof field, in->uptime_s);
        place(line1, 0, "Up");
        place_right(line1, field);
        return;
    }

    format_temp(field, sizeof field, in->temp_c_x100, in->units);
    place(line0, 0, field);
    format_humidity(field, sizeof field, in->rh_x100);
    place_right(line0, field);
    format_pressure(field, sizeof field, in->pressure_pa, in->units);
    place(line1, 0, field);
    field[0] = tendency_mark(in->tendency);
    field[1] = '\0';
    place_right(line1, field);
}
