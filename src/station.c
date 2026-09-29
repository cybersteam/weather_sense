#include "ws/station.h"

#include "ws/bme280.h"
#include "ws/config_store.h"
#include "ws/faults.h"
#include "ws/flashstr.h"
#include "ws/format.h"
#include "ws/hal.h"
#include "ws/keypad.h"
#include "ws/lcd.h"
#include "ws/metrics.h"
#include "ws/protocol.h"
#include "ws/ui.h"
#include "ws/version.h"

#define LINE_MAX 48u
#define KEY_SAMPLE_MS 10u
#define KEY_STABLE_SAMPLES 3u
#define PAGE_MS 3000u

typedef struct {
    ws_config_t cfg;
    bool dirty;
    ws_bme280_t bme;
    ws_reading_t last;
    bool have_sample;
    uint32_t last_ok_ms;
    uint32_t last_request_ms;
    bool requested_once;
    uint32_t boot_ms;
    uint32_t samples;
    uint16_t faults;
    ws_tendency_t tendency;
    int8_t tend;
    int16_t dew_c_x100;
    int16_t heat_c_x100;
    bool heat_applicable;
    uint8_t page;
    bool hold;
    bool editing;
    int16_t edit_alt;
    char line[LINE_MAX];
    uint8_t line_len;
    bool line_overflow;
    uint32_t last_key_ms;
    ws_key_t raw_key;
    uint8_t raw_count;
    ws_key_t stable_key;
    uint32_t last_rotate_ms;
} station_t;

static station_t st;

static int16_t clamp_alt(int32_t value)
{
    if (value < WS_ALT_MIN_M) {
        return (int16_t)WS_ALT_MIN_M;
    }
    if (value > WS_ALT_MAX_M) {
        return (int16_t)WS_ALT_MAX_M;
    }
    return (int16_t)value;
}

static void puts_reset(ws_reset_reason_t reason)
{
    switch (reason) {
    case WS_RESET_POWER_ON:
        ws_uart_puts_P(WS_PSTR("POR"));
        break;
    case WS_RESET_EXTERNAL:
        ws_uart_puts_P(WS_PSTR("EXT"));
        break;
    case WS_RESET_BROWNOUT:
        ws_uart_puts_P(WS_PSTR("BOR"));
        break;
    case WS_RESET_WATCHDOG:
        ws_uart_puts_P(WS_PSTR("WDT"));
        break;
    default:
        ws_uart_puts_P(WS_PSTR("UNK"));
        break;
    }
}

static void emit_telemetry(void)
{
    ws_telem_t telem;
    char line[96];
    size_t n;

    telem.temp_c_x100 = st.last.temperature_c_x100;
    telem.rh_x100 = st.last.humidity_rh_x100;
    telem.pressure_pa = st.last.pressure_pa;
    telem.sea_level_pa = ws_sea_level_pa(st.last.pressure_pa,
                                          (int16_t)st.last.temperature_c_x100,
                                          st.editing ? st.edit_alt : st.cfg.altitude_m);
    telem.dew_c_x100 = st.dew_c_x100;
    telem.heat_c_x100 = st.heat_c_x100;
    telem.heat_applicable = st.heat_applicable;
    telem.tendency = st.tend;
    telem.faults = st.faults;
    telem.units = st.cfg.units;
    n = ws_format_telemetry(line, sizeof line, &telem);
    if (n >= sizeof line) {
        n = sizeof line - 1u;
    }
    ws_uart_write((const uint8_t *)line, n);
}

static void note_sample(const ws_reading_t *reading, uint32_t now)
{
    st.last = *reading;
    st.have_sample = true;
    st.last_ok_ms = now;
    st.samples++;
    st.faults &= (uint16_t)~(WS_FAULT_BME_ID | WS_FAULT_BME_TIMEOUT | WS_FAULT_BME_RANGE |
                              WS_FAULT_BME_STALE);
    st.dew_c_x100 = ws_dew_point_c_x100((int16_t)reading->temperature_c_x100,
                                        reading->humidity_rh_x100);
    st.heat_c_x100 = ws_heat_index_c_x100((int16_t)reading->temperature_c_x100,
                                          reading->humidity_rh_x100, &st.heat_applicable);
    st.tend = ws_tendency_update(&st.tendency, reading->pressure_pa, now);
    if (st.cfg.telemetry != 0u) {
        emit_telemetry();
    }
}

static void refresh_stale(uint32_t now)
{
    uint32_t limit = (uint32_t)st.cfg.period_ms * 3u;
    uint32_t age;

    if ((st.faults & WS_FAULT_BME_ID) != 0u) {
        return;
    }
    if (limit < 3000u) {
        limit = 3000u;
    }
    age = st.have_sample ? (now - st.last_ok_ms) : (now - st.boot_ms);
    if (age > limit) {
        st.faults |= WS_FAULT_BME_STALE;
    }
}

static void service_sensor(uint32_t now)
{
    ws_reading_t reading;
    ws_bme_status_t status;
    bool due;

    /* Request before poll so a conversion that finishes inside the SPI
     * write (the host fake, and a sensor that is already done) is
     * published in this same call. A conversion still measuring returns
     * BUSY and is completed or timed out on a later poll. */
    due = !st.requested_once || (uint32_t)(now - st.last_request_ms) >= st.cfg.period_ms;
    if (st.bme.ready && !st.bme.awaiting && due) {
        ws_bme280_request(&st.bme);
        st.last_request_ms = now;
        st.requested_once = true;
    }

    status = ws_bme280_poll(&st.bme, &reading);
    if (status == WS_BME_OK) {
        note_sample(&reading, now);
    } else if (status == WS_BME_ERR_TIMEOUT) {
        st.faults |= WS_FAULT_BME_TIMEOUT;
    } else if (status == WS_BME_ERR_RANGE) {
        st.faults |= WS_FAULT_BME_RANGE;
    } else if (status == WS_BME_ERR_ID) {
        st.faults |= WS_FAULT_BME_ID;
    }
    refresh_stale(now);
}

static void draw(uint32_t now)
{
    ws_ui_input_t ui;
    char line0[17];
    char line1[17];
    int16_t altitude = st.editing ? st.edit_alt : st.cfg.altitude_m;

    ui.have_sample = st.have_sample;
    ui.temp_c_x100 = st.last.temperature_c_x100;
    ui.rh_x100 = st.last.humidity_rh_x100;
    ui.pressure_pa = st.last.pressure_pa;
    ui.sea_level_pa = st.have_sample
                          ? ws_sea_level_pa(st.last.pressure_pa,
                                            (int16_t)st.last.temperature_c_x100, altitude)
                          : 0u;
    ui.dew_c_x100 = st.dew_c_x100;
    ui.heat_c_x100 = st.heat_c_x100;
    ui.heat_applicable = st.heat_applicable;
    ui.tendency = st.tend;
    ui.page = st.page;
    ui.hold = st.hold;
    ui.editing = st.editing;
    ui.altitude_m = altitude;
    ui.units = st.cfg.units;
    ui.uptime_s = (now - st.boot_ms) / 1000u;
    ui.faults = st.faults;
    ui.reset_reason = ws_reset_reason();
    ws_ui_format(&ui, line0, line1);
    ws_lcd_draw(line0, line1);
}

static void cmd_help(void)
{
    ws_uart_puts_P(WS_PSTR("OK\n"));
    ws_uart_puts_P(WS_PSTR("VER READ STATUS CFG\n"));
    ws_uart_puts_P(WS_PSTR("SET UNIT M|I\n"));
    ws_uart_puts_P(WS_PSTR("SET ALT <m>\n"));
    ws_uart_puts_P(WS_PSTR("SET PERIOD <ms>\n"));
    ws_uart_puts_P(WS_PSTR("SET TELEM ON|OFF\n"));
    ws_uart_puts_P(WS_PSTR("SAVE LOAD DEFAULTS\n"));
    ws_uart_puts_P(WS_PSTR("PAGE HOLD RESET\n"));
}

static void cmd_status(uint32_t now)
{
    char num[16];

    ws_uart_puts_P(WS_PSTR("OK up="));
    ws_fmt_u32(num, sizeof num, (now - st.boot_ms) / 1000u);
    ws_uart_puts(num);
    ws_uart_puts_P(WS_PSTR(" samples="));
    ws_fmt_u32(num, sizeof num, st.samples);
    ws_uart_puts(num);
    ws_uart_puts_P(WS_PSTR(" faults="));
    ws_fmt_hex16(num, sizeof num, 0, st.faults);
    ws_uart_puts(num);
    ws_uart_puts_P(WS_PSTR(" dirty="));
    ws_uart_puts_P(st.dirty ? WS_PSTR("1") : WS_PSTR("0"));
    ws_uart_puts_P(WS_PSTR(" drops="));
    ws_fmt_u32(num, sizeof num, ws_uart_tx_drops());
    ws_uart_puts(num);
    ws_uart_puts_P(WS_PSTR("\n"));
}

static void cmd_cfg(void)
{
    char num[16];

    ws_uart_puts_P(WS_PSTR("OK unit="));
    ws_uart_puts_P(st.cfg.units == WS_UNIT_IMPERIAL ? WS_PSTR("I") : WS_PSTR("M"));
    ws_uart_puts_P(WS_PSTR(" alt="));
    ws_fmt_i32(num, sizeof num, st.cfg.altitude_m);
    ws_uart_puts(num);
    ws_uart_puts_P(WS_PSTR(" period="));
    ws_fmt_u32(num, sizeof num, st.cfg.period_ms);
    ws_uart_puts(num);
    ws_uart_puts_P(WS_PSTR(" telem="));
    ws_uart_puts_P(st.cfg.telemetry ? WS_PSTR("1\n") : WS_PSTR("0\n"));
}

static void apply_config_status(ws_cfg_status_t status)
{
    if (status == WS_CFG_CORRUPT) {
        st.faults |= WS_FAULT_CONFIG;
        st.dirty = true;
        return;
    }
    st.faults &= (uint16_t)~WS_FAULT_CONFIG;
    st.dirty = false;
}

static void handle_command(const ws_command_t *command, uint32_t now)
{
    char num[16];

    switch (command->id) {
    case WS_CMD_NONE:
        break;
    case WS_CMD_HELP:
        cmd_help();
        break;
    case WS_CMD_VER:
        ws_uart_puts_P(WS_PSTR("OK " WS_PRODUCT " " WS_VERSION_STR " " WS_MCU " reset="));
        puts_reset(ws_reset_reason());
        ws_uart_puts_P(WS_PSTR("\n"));
        break;
    case WS_CMD_READ:
        if (!st.have_sample) {
            ws_uart_puts_P(WS_PSTR("ERR no-sample\n"));
        } else {
            emit_telemetry();
        }
        break;
    case WS_CMD_STATUS:
        cmd_status(now);
        break;
    case WS_CMD_CFG:
        cmd_cfg();
        break;
    case WS_CMD_SET_UNIT:
        st.cfg.units = (uint8_t)command->arg;
        st.dirty = true;
        ws_uart_puts_P(WS_PSTR("OK\n"));
        break;
    case WS_CMD_SET_ALT:
        if (command->arg < WS_ALT_MIN_M || command->arg > WS_ALT_MAX_M) {
            ws_uart_puts_P(WS_PSTR("ERR range\n"));
            break;
        }
        st.cfg.altitude_m = (int16_t)command->arg;
        st.dirty = true;
        ws_uart_puts_P(WS_PSTR("OK\n"));
        break;
    case WS_CMD_SET_PERIOD:
        if (command->arg < (int32_t)WS_PERIOD_MIN_MS ||
            command->arg > (int32_t)WS_PERIOD_MAX_MS) {
            ws_uart_puts_P(WS_PSTR("ERR range\n"));
            break;
        }
        st.cfg.period_ms = (uint16_t)command->arg;
        st.dirty = true;
        ws_uart_puts_P(WS_PSTR("OK\n"));
        break;
    case WS_CMD_SET_TELEM:
        st.cfg.telemetry = (uint8_t)command->arg;
        st.dirty = true;
        ws_uart_puts_P(WS_PSTR("OK\n"));
        break;
    case WS_CMD_SAVE:
        if (!ws_config_save(&st.cfg)) {
            ws_uart_puts_P(WS_PSTR("ERR range\n"));
            break;
        }
        st.dirty = false;
        st.faults &= (uint16_t)~WS_FAULT_CONFIG;
        ws_uart_puts_P(WS_PSTR("OK saved\n"));
        break;
    case WS_CMD_LOAD:
        apply_config_status(ws_config_load(&st.cfg));
        if ((st.faults & WS_FAULT_CONFIG) != 0u) {
            ws_uart_puts_P(WS_PSTR("ERR crc\n"));
        } else {
            ws_uart_puts_P(WS_PSTR("OK loaded\n"));
        }
        break;
    case WS_CMD_DEFAULTS:
        ws_config_defaults(&st.cfg);
        st.dirty = true;
        ws_uart_puts_P(WS_PSTR("OK defaults\n"));
        break;
    case WS_CMD_PAGE:
        st.page = (uint8_t)((st.page + 1u) % WS_UI_PAGES);
        st.last_rotate_ms = now;
        st.editing = false;
        ws_uart_puts_P(WS_PSTR("OK page="));
        ws_fmt_u32(num, sizeof num, st.page);
        ws_uart_puts(num);
        ws_uart_puts_P(WS_PSTR("\n"));
        break;
    case WS_CMD_HOLD:
        st.hold = !st.hold;
        ws_uart_puts_P(st.hold ? WS_PSTR("OK hold=1\n") : WS_PSTR("OK hold=0\n"));
        break;
    case WS_CMD_RESET:
        ws_uart_puts_P(WS_PSTR("OK reset\n"));
        ws_system_reset();
        break;
    case WS_CMD_UNKNOWN:
        ws_uart_puts_P(WS_PSTR("ERR unknown\n"));
        break;
    case WS_CMD_SYNTAX:
        ws_uart_puts_P(WS_PSTR("ERR syntax\n"));
        break;
    default:
        ws_uart_puts_P(WS_PSTR("ERR unknown\n"));
        break;
    }
}

static void take_line(uint32_t now)
{
    st.line[st.line_len] = '\0';
    if (st.line_overflow || st.line_len == 0u) {
        if (st.line_overflow) {
            ws_uart_puts_P(WS_PSTR("ERR syntax\n"));
        }
    } else {
        ws_command_t command = ws_parse_command(st.line);
        handle_command(&command, now);
    }
    st.line_len = 0u;
    st.line_overflow = false;
}

static void service_uart(uint32_t now)
{
    uint8_t buf[16];
    size_t n = ws_uart_read(buf, sizeof buf);
    size_t i;

    for (i = 0; i < n; i++) {
        uint8_t byte = buf[i];
        if (byte == '\r') {
            continue;
        }
        if (byte == '\n') {
            take_line(now);
            continue;
        }
        if (st.line_len + 1u >= LINE_MAX) {
            st.line_overflow = true;
            continue;
        }
        st.line[st.line_len++] = (char)byte;
    }
}

static void on_key(ws_key_t key, uint32_t now)
{
    if (key == WS_KEY_NONE) {
        return;
    }
    if (st.editing) {
        if (key == WS_KEY_UP) {
            st.edit_alt = clamp_alt((int32_t)st.edit_alt + 1);
        } else if (key == WS_KEY_DOWN) {
            st.edit_alt = clamp_alt((int32_t)st.edit_alt - 1);
        } else if (key == WS_KEY_RIGHT) {
            st.edit_alt = clamp_alt((int32_t)st.edit_alt + 10);
        } else if (key == WS_KEY_LEFT) {
            st.edit_alt = clamp_alt((int32_t)st.edit_alt - 10);
        } else if (key == WS_KEY_SELECT) {
            st.cfg.altitude_m = st.edit_alt;
            st.editing = false;
            if (ws_config_save(&st.cfg)) {
                st.dirty = false;
                st.faults &= (uint16_t)~WS_FAULT_CONFIG;
            }
        }
        return;
    }
    if (key == WS_KEY_RIGHT || key == WS_KEY_LEFT) {
        if (key == WS_KEY_RIGHT) {
            st.page = (uint8_t)((st.page + 1u) % WS_UI_PAGES);
        } else {
            st.page = (uint8_t)((st.page + WS_UI_PAGES - 1u) % WS_UI_PAGES);
        }
        st.last_rotate_ms = now;
        return;
    }
    if (key == WS_KEY_SELECT) {
        st.hold = !st.hold;
        return;
    }
    if (key == WS_KEY_UP || key == WS_KEY_DOWN) {
        st.editing = true;
        st.edit_alt = st.cfg.altitude_m;
        if (key == WS_KEY_UP) {
            st.edit_alt = clamp_alt((int32_t)st.edit_alt + 1);
        } else {
            st.edit_alt = clamp_alt((int32_t)st.edit_alt - 1);
        }
    }
}

static void service_keys(uint32_t now)
{
    ws_key_t key;

    if ((uint32_t)(now - st.last_key_ms) < KEY_SAMPLE_MS) {
        return;
    }
    st.last_key_ms = now;
    key = ws_keypad_decode(ws_adc_read());
    if (key == st.raw_key) {
        if (st.raw_count < 255u) {
            st.raw_count++;
        }
    } else {
        st.raw_key = key;
        st.raw_count = 1u;
    }
    if (st.raw_count >= KEY_STABLE_SAMPLES && key != st.stable_key) {
        st.stable_key = key;
        on_key(key, now);
    }
}

static void rotate_page(uint32_t now)
{
    if (st.hold || st.editing) {
        return;
    }
    if ((uint32_t)(now - st.last_rotate_ms) < PAGE_MS) {
        return;
    }
    st.page = (uint8_t)((st.page + 1u) % WS_UI_PAGES);
    st.last_rotate_ms = now;
}

void ws_station_init(void)
{
    ws_bme_status_t sensor;
    ws_reset_reason_t reset = ws_reset_reason();
    uint32_t now;

    st.dirty = false;
    st.have_sample = false;
    st.last_ok_ms = 0u;
    st.last_request_ms = 0u;
    st.requested_once = false;
    st.samples = 0u;
    st.faults = 0u;
    st.tend = WS_TEND_UNKNOWN;
    st.dew_c_x100 = 0;
    st.heat_c_x100 = 0;
    st.heat_applicable = false;
    st.page = 0u;
    st.hold = false;
    st.editing = false;
    st.edit_alt = 0;
    st.line_len = 0u;
    st.line_overflow = false;
    st.raw_key = WS_KEY_NONE;
    st.raw_count = 0u;
    st.stable_key = WS_KEY_NONE;
    st.last.temperature_c_x100 = 0;
    st.last.pressure_pa = 0u;
    st.last.humidity_rh_x100 = 0u;
    st.last.valid = false;
    ws_tendency_init(&st.tendency);

    if (reset == WS_RESET_WATCHDOG) {
        st.faults |= WS_FAULT_WDT_RESET;
    } else if (reset == WS_RESET_BROWNOUT) {
        st.faults |= WS_FAULT_BROWNOUT;
    }
    apply_config_status(ws_config_load(&st.cfg));
    /* A factory-blank image is not a fault and is not dirty. A corrupt image is. */
    if ((st.faults & WS_FAULT_CONFIG) == 0u) {
        st.dirty = false;
    }

    now = ws_millis();
    st.boot_ms = now;
    st.last_key_ms = now;
    st.last_rotate_ms = now;

    ws_lcd_init();
    sensor = ws_bme280_bootstrap(&st.bme);
    if (sensor == WS_BME_ERR_ID) {
        st.faults |= WS_FAULT_BME_ID;
    } else if (sensor != WS_BME_OK) {
        st.faults |= WS_FAULT_BME_TIMEOUT;
    }

    ws_uart_puts_P(WS_PSTR(WS_PRODUCT " " WS_VERSION_STR "\n"));
    ws_uart_puts_P(WS_PSTR("reset="));
    puts_reset(reset);
    ws_uart_puts_P(WS_PSTR("\n"));
    draw(now);
}

void ws_station_poll(void)
{
    uint32_t now = ws_millis();

    service_uart(now);
    service_keys(now);
    service_sensor(now);
    rotate_page(now);
    draw(now);
}

uint16_t ws_station_faults(void)
{
    return st.faults;
}

uint32_t ws_station_samples(void)
{
    return st.samples;
}
