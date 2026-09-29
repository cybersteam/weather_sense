#pragma once

#include <stdbool.h>
#include <stdint.h>

#define WS_CFG_VERSION 1u
#define WS_CFG_BYTES 12u
#define WS_CFG_EEPROM_BASE 0u

#define WS_UNIT_METRIC 0u
#define WS_UNIT_IMPERIAL 1u

#define WS_ALT_MIN_M (-400)
#define WS_ALT_MAX_M 4500
#define WS_PERIOD_MIN_MS 500u
#define WS_PERIOD_MAX_MS 60000u
#define WS_PERIOD_DEFAULT_MS 1000u

typedef struct {
    uint8_t units;
    int16_t altitude_m;
    uint16_t period_ms;
    uint8_t telemetry;
} ws_config_t;

typedef enum {
    WS_CFG_OK = 0,
    WS_CFG_BLANK,    /* erased EEPROM, factory state, not a fault */
    WS_CFG_CORRUPT   /* magic present or garbage that fails the CRC */
} ws_cfg_status_t;

void ws_config_defaults(ws_config_t *cfg);
bool ws_config_in_range(const ws_config_t *cfg);
ws_cfg_status_t ws_config_load(ws_config_t *cfg);
bool ws_config_save(const ws_config_t *cfg);
