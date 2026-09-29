#pragma once

#include <stdint.h>

void ws_station_init(void);
void ws_station_poll(void);
uint16_t ws_station_faults(void);
uint32_t ws_station_samples(void);
