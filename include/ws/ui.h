#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "ws/hal.h"

#define WS_UI_PAGES 4

typedef struct {
    bool have_sample;
    int32_t temp_c_x100;
    uint16_t rh_x100;
    uint32_t pressure_pa;
    uint32_t sea_level_pa;
    int16_t dew_c_x100;
    int16_t heat_c_x100;
    bool heat_applicable;
    int8_t tendency;
    uint8_t page;
    bool hold;
    bool editing;
    int16_t altitude_m;
    uint8_t units;
    uint32_t uptime_s;
    uint16_t faults;
    ws_reset_reason_t reset_reason;
} ws_ui_input_t;

/* Each line is exactly 16 characters plus a NUL. */
void ws_ui_format(const ws_ui_input_t *in, char line0[17], char line1[17]);
