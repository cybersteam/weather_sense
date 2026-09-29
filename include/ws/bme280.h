#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * BME280 in SPI forced mode, weather-monitoring profile from the Bosch
 * datasheet: oversampling x1, IIR filter off. Compensation is the published
 * integer formulas (BST-BME280-DS002 section 4.2.3), not a vendor library.
 */

typedef struct {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
    uint8_t dig_H1;
    int16_t dig_H2;
    int8_t dig_H3;
    int16_t dig_H4;
    int16_t dig_H5;
    int8_t dig_H6;
    int32_t t_fine;
} ws_bme_cal_t;

typedef enum {
    WS_BME_OK = 0,
    WS_BME_BUSY,
    WS_BME_ERR_ID,
    WS_BME_ERR_TIMEOUT,
    WS_BME_ERR_RANGE
} ws_bme_status_t;

typedef struct {
    int32_t temperature_c_x100;
    uint32_t pressure_pa;
    uint16_t humidity_rh_x100;
    bool valid;
} ws_reading_t;

typedef struct {
    ws_bme_cal_t cal;
    bool ready;
    bool awaiting;
    uint32_t request_ms;
} ws_bme280_t;

void ws_bme280_init(ws_bme280_t *dev);
ws_bme_status_t ws_bme280_bootstrap(ws_bme280_t *dev);
void ws_bme280_request(ws_bme280_t *dev);
ws_bme_status_t ws_bme280_poll(ws_bme280_t *dev, ws_reading_t *out);

void ws_bme_parse_calibration(ws_bme_cal_t *cal, const uint8_t block_88[26],
                              const uint8_t block_e1[7]);
void ws_bme_compensate(ws_bme_cal_t *cal, int32_t adc_t, int32_t adc_p,
                       int32_t adc_h, int32_t *temp_c_x100,
                       uint32_t *pressure_q24_8, uint32_t *humidity_q22_10);

uint32_t ws_bme_pa_from_q24_8(uint32_t q24_8);
uint16_t ws_bme_rh_x100_from_q22_10(uint32_t q22_10);
