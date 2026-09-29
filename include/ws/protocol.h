#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    WS_CMD_NONE = 0,
    WS_CMD_HELP,
    WS_CMD_VER,
    WS_CMD_READ,
    WS_CMD_STATUS,
    WS_CMD_CFG,
    WS_CMD_SET_UNIT,
    WS_CMD_SET_ALT,
    WS_CMD_SET_PERIOD,
    WS_CMD_SET_TELEM,
    WS_CMD_SAVE,
    WS_CMD_LOAD,
    WS_CMD_DEFAULTS,
    WS_CMD_PAGE,
    WS_CMD_HOLD,
    WS_CMD_RESET,
    WS_CMD_UNKNOWN,
    WS_CMD_SYNTAX
} ws_cmd_id_t;

typedef struct {
    ws_cmd_id_t id;
    int32_t arg;
} ws_command_t;

/* One LF-terminated service command. CR is not required. Case-insensitive. */
ws_command_t ws_parse_command(const char *line);

typedef struct {
    int32_t temp_c_x100;
    uint16_t rh_x100;
    uint32_t pressure_pa;
    uint32_t sea_level_pa;
    int16_t dew_c_x100;
    int16_t heat_c_x100;
    bool heat_applicable;
    int8_t tendency;
    uint16_t faults;
    uint8_t units;
} ws_telem_t;

/* Writes one telemetry line, including the trailing LF. Scales are SI. */
size_t ws_format_telemetry(char *dst, size_t cap, const ws_telem_t *in);
