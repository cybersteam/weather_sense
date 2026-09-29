#include "ws/config_store.h"

#include "ws/crc16.h"
#include "ws/hal.h"

void ws_config_defaults(ws_config_t *cfg)
{
    cfg->units = WS_UNIT_METRIC;
    cfg->altitude_m = 0;
    cfg->period_ms = WS_PERIOD_DEFAULT_MS;
    cfg->telemetry = 1u;
}

bool ws_config_in_range(const ws_config_t *cfg)
{
    if (cfg->units != WS_UNIT_METRIC && cfg->units != WS_UNIT_IMPERIAL) {
        return false;
    }
    if (cfg->altitude_m < WS_ALT_MIN_M || cfg->altitude_m > WS_ALT_MAX_M) {
        return false;
    }
    if (cfg->period_ms < WS_PERIOD_MIN_MS || cfg->period_ms > WS_PERIOD_MAX_MS) {
        return false;
    }
    if (cfg->telemetry > 1u) {
        return false;
    }
    return true;
}

static void store_u16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static uint16_t load_u16(const uint8_t *src)
{
    return (uint16_t)src[0] | (uint16_t)((uint16_t)src[1] << 8);
}

static void pack(const ws_config_t *cfg, uint8_t raw[WS_CFG_BYTES])
{
    uint16_t crc;
    uint8_t i;

    for (i = 0; i < WS_CFG_BYTES; i++) {
        raw[i] = 0u;
    }
    raw[0] = (uint8_t)'W';
    raw[1] = (uint8_t)'S';
    raw[2] = WS_CFG_VERSION;
    raw[3] = cfg->units;
    store_u16(&raw[4], (uint16_t)cfg->altitude_m);
    store_u16(&raw[6], cfg->period_ms);
    raw[8] = cfg->telemetry;
    raw[9] = 0u;
    crc = ws_crc16(raw, 10u);
    store_u16(&raw[10], crc);
}

static bool unpack(const uint8_t raw[WS_CFG_BYTES], ws_config_t *cfg)
{
    uint16_t crc = ws_crc16(raw, 10u);
    uint16_t stored = load_u16(&raw[10]);
    int16_t altitude;

    if (crc != stored || raw[0] != (uint8_t)'W' || raw[1] != (uint8_t)'S') {
        return false;
    }
    if (raw[2] != WS_CFG_VERSION) {
        return false;
    }
    altitude = (int16_t)load_u16(&raw[4]);
    cfg->units = raw[3];
    cfg->altitude_m = altitude;
    cfg->period_ms = load_u16(&raw[6]);
    cfg->telemetry = raw[8];
    return ws_config_in_range(cfg);
}

static bool image_blank(const uint8_t raw[WS_CFG_BYTES])
{
    uint8_t i;

    for (i = 0; i < WS_CFG_BYTES; i++) {
        if (raw[i] != 0xFFu) {
            return false;
        }
    }
    return true;
}

ws_cfg_status_t ws_config_load(ws_config_t *cfg)
{
    uint8_t raw[WS_CFG_BYTES];
    uint8_t i;

    for (i = 0; i < WS_CFG_BYTES; i++) {
        raw[i] = ws_eeprom_read((uint16_t)(WS_CFG_EEPROM_BASE + i));
    }
    if (image_blank(raw)) {
        ws_config_defaults(cfg);
        return WS_CFG_BLANK;
    }
    if (!unpack(raw, cfg)) {
        ws_config_defaults(cfg);
        return WS_CFG_CORRUPT;
    }
    return WS_CFG_OK;
}

bool ws_config_save(const ws_config_t *cfg)
{
    uint8_t raw[WS_CFG_BYTES];
    uint8_t i;

    if (!ws_config_in_range(cfg)) {
        return false;
    }
    pack(cfg, raw);
    for (i = 0; i < WS_CFG_BYTES; i++) {
        ws_wdt_kick();
        ws_eeprom_write((uint16_t)(WS_CFG_EEPROM_BASE + i), raw[i]);
    }
    return true;
}
