#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Short-term pressure tendency. 9 means the window is not full yet. */
#define WS_TEND_FALLING (-1)
#define WS_TEND_STEADY 0
#define WS_TEND_RISING 1
#define WS_TEND_UNKNOWN 9

/* Magnus dew point over water, hundredths of a degree Celsius. */
int16_t ws_dew_point_c_x100(int16_t temp_c_x100, uint16_t rh_x100);

/*
 * NOAA heat index. `applicable` is true when the Rothfusz regression is in
 * its valid region; below that the return value is the Steadman simple
 * result and the display shows the index as not applicable.
 */
int16_t ws_heat_index_f_x100(int16_t temp_f_x100, uint16_t rh_x100,
                             bool *applicable);
int16_t ws_heat_index_c_x100(int16_t temp_c_x100, uint16_t rh_x100,
                             bool *applicable);

/* Isothermal hypsometric reduction of station pressure to sea level. */
uint32_t ws_sea_level_pa(uint32_t station_pa, int16_t temp_c_x100,
                         int16_t altitude_m);

int32_t ws_c_to_f_x100(int32_t temp_c_x100);
int32_t ws_f_to_c_x100(int32_t temp_f_x100);

#define WS_TEND_POINTS 16
#define WS_TEND_INTERVAL_MS 60000u
#define WS_TEND_THRESHOLD_HPA_X10 5 /* 0.5 hPa across the window */

typedef struct {
    uint16_t hpa_x10[WS_TEND_POINTS];
    uint8_t count;
    uint8_t next;
    uint32_t last_record_ms;
    bool started;
    int8_t current;
} ws_tendency_t;

void ws_tendency_init(ws_tendency_t *tend);
int8_t ws_tendency_update(ws_tendency_t *tend, uint32_t pressure_pa,
                          uint32_t now_ms);
