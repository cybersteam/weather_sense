#include "ws/bme280.h"

#include "ws/hal.h"

#define BME_REG_CALIB 0x88u
#define BME_REG_ID 0xD0u
#define BME_REG_RESET 0xE0u
#define BME_REG_CALIB_H 0xE1u
#define BME_REG_CTRL_HUM 0xF2u
#define BME_REG_STATUS 0xF3u
#define BME_REG_CTRL_MEAS 0xF4u
#define BME_REG_CONFIG 0xF5u
#define BME_REG_DATA 0xF7u

#define BME_CHIP_ID 0x60u
#define BME_SOFT_RESET 0xB6u
#define BME_STATUS_MEASURING 0x08u
#define BME_STATUS_IM_UPDATE 0x01u

/* osrs_t x1, osrs_p x1. Humidity x1 is latched by the following ctrl_meas write. */
#define BME_CTRL_HUM 0x01u
#define BME_CTRL_SLEEP 0x24u
#define BME_CTRL_FORCED 0x25u
#define BME_CONFIG_WEATHER 0x00u
#define BME_CONV_TIMEOUT_MS 30u

static void cs_low(void)
{
    ws_gpio_set(WS_PIN_BME_CS, false);
}

static void cs_high(void)
{
    ws_gpio_set(WS_PIN_BME_CS, true);
}

static uint8_t read_reg(uint8_t reg)
{
    uint8_t value;

    cs_low();
    (void)ws_spi_xfer((uint8_t)(reg | 0x80u));
    value = ws_spi_xfer(0xFFu);
    cs_high();
    return value;
}

static void write_reg(uint8_t reg, uint8_t value)
{
    cs_low();
    (void)ws_spi_xfer((uint8_t)(reg & 0x7Fu));
    (void)ws_spi_xfer(value);
    cs_high();
}

static void read_regs(uint8_t reg, uint8_t *buf, uint8_t n)
{
    uint8_t i;

    cs_low();
    (void)ws_spi_xfer((uint8_t)(reg | 0x80u));
    for (i = 0; i < n; i++) {
        buf[i] = ws_spi_xfer(0xFFu);
    }
    cs_high();
}

static int16_t le_i16(uint8_t lo, uint8_t hi)
{
    return (int16_t)((uint16_t)lo | (uint16_t)((uint16_t)hi << 8));
}

/* Two's-complement left shift. A signed shift of a negative value is
 * undefined in C; the Bosch formulas rely on the bit pattern. */
static int32_t shl32(int32_t value, unsigned shift)
{
    return (int32_t)((uint32_t)value << shift);
}

static int64_t shl64(int64_t value, unsigned shift)
{
    return (int64_t)((uint64_t)value << shift);
}

static int16_t sign_extend_12(uint16_t value)
{
    value = (uint16_t)(value & 0x0FFFu);
    if ((value & 0x0800u) != 0u) {
        value = (uint16_t)(value | 0xF000u);
    }
    return (int16_t)value;
}

void ws_bme_parse_calibration(ws_bme_cal_t *cal, const uint8_t block_88[26],
                              const uint8_t block_e1[7])
{
    cal->dig_T1 = (uint16_t)((uint16_t)block_88[0] | (uint16_t)((uint16_t)block_88[1] << 8));
    cal->dig_T2 = le_i16(block_88[2], block_88[3]);
    cal->dig_T3 = le_i16(block_88[4], block_88[5]);
    cal->dig_P1 = (uint16_t)((uint16_t)block_88[6] | (uint16_t)((uint16_t)block_88[7] << 8));
    cal->dig_P2 = le_i16(block_88[8], block_88[9]);
    cal->dig_P3 = le_i16(block_88[10], block_88[11]);
    cal->dig_P4 = le_i16(block_88[12], block_88[13]);
    cal->dig_P5 = le_i16(block_88[14], block_88[15]);
    cal->dig_P6 = le_i16(block_88[16], block_88[17]);
    cal->dig_P7 = le_i16(block_88[18], block_88[19]);
    cal->dig_P8 = le_i16(block_88[20], block_88[21]);
    cal->dig_P9 = le_i16(block_88[22], block_88[23]);
    cal->dig_H1 = block_88[25];
    cal->dig_H2 = le_i16(block_e1[0], block_e1[1]);
    cal->dig_H3 = (int8_t)block_e1[2];
    /* H4 and H5 are 12-bit signed fields split across 0xE4..0xE6. */
    cal->dig_H4 = sign_extend_12((uint16_t)(((uint16_t)block_e1[3] << 4) | (block_e1[4] & 0x0Fu)));
    cal->dig_H5 = sign_extend_12((uint16_t)(((uint16_t)block_e1[5] << 4) | (uint16_t)(block_e1[4] >> 4)));
    cal->dig_H6 = (int8_t)block_e1[6];
    cal->t_fine = 0;
}

static int32_t compensate_t(ws_bme_cal_t *cal, int32_t adc_t)
{
    int32_t var1;
    int32_t var2;
    int32_t diff;

    var1 = ((((adc_t >> 3) - ((int32_t)cal->dig_T1 << 1))) * (int32_t)cal->dig_T2) >> 11;
    diff = (adc_t >> 4) - (int32_t)cal->dig_T1;
    var2 = (((diff * diff) >> 12) * (int32_t)cal->dig_T3) >> 14;
    cal->t_fine = var1 + var2;
    return (cal->t_fine * 5 + 128) >> 8;
}

static uint32_t compensate_p(const ws_bme_cal_t *cal, int32_t adc_p)
{
    int64_t var1;
    int64_t var2;
    int64_t p;

    var1 = (int64_t)cal->t_fine - 128000;
    var2 = var1 * var1 * (int64_t)cal->dig_P6;
    var2 = var2 + shl64(var1 * (int64_t)cal->dig_P5, 17);
    var2 = var2 + shl64((int64_t)cal->dig_P4, 35);
    var1 = ((var1 * var1 * (int64_t)cal->dig_P3) >> 8) + shl64(var1 * (int64_t)cal->dig_P2, 12);
    var1 = (((((int64_t)1) << 47) + var1) * (int64_t)cal->dig_P1) >> 33;
    if (var1 == 0) {
        return 0u;
    }
    p = 1048576 - (int64_t)adc_p;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)cal->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)cal->dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)cal->dig_P7) << 4);
    if (p < 0) {
        return 0u;
    }
    return (uint32_t)p;
}

static uint32_t compensate_h(const ws_bme_cal_t *cal, int32_t adc_h)
{
    int32_t v;

    v = cal->t_fine - (int32_t)76800;
    v = (((((adc_h << 14) - shl32((int32_t)cal->dig_H4, 20) - (((int32_t)cal->dig_H5) * v)) +
           (int32_t)16384) >>
          15) *
         (((((((v * (int32_t)cal->dig_H6) >> 10) *
              (((v * (int32_t)cal->dig_H3) >> 11) + (int32_t)32768)) >>
             10) +
            (int32_t)2097152) *
               (int32_t)cal->dig_H2 +
           8192) >>
          14));
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)cal->dig_H1) >> 4);
    if (v < 0) {
        v = 0;
    }
    if (v > 419430400) {
        v = 419430400;
    }
    return (uint32_t)(v >> 12);
}

void ws_bme_compensate(ws_bme_cal_t *cal, int32_t adc_t, int32_t adc_p, int32_t adc_h,
                       int32_t *temp_c_x100, uint32_t *pressure_q24_8,
                       uint32_t *humidity_q22_10)
{
    *temp_c_x100 = compensate_t(cal, adc_t);
    *pressure_q24_8 = compensate_p(cal, adc_p);
    *humidity_q22_10 = compensate_h(cal, adc_h);
}

uint32_t ws_bme_pa_from_q24_8(uint32_t q24_8)
{
    return (q24_8 + 128u) >> 8;
}

uint16_t ws_bme_rh_x100_from_q22_10(uint32_t q22_10)
{
    uint32_t rh = (q22_10 * 100u + 512u) / 1024u;

    if (rh > 10000u) {
        rh = 10000u;
    }
    return (uint16_t)rh;
}

void ws_bme280_init(ws_bme280_t *dev)
{
    dev->ready = false;
    dev->awaiting = false;
    dev->request_ms = 0u;
    dev->cal.t_fine = 0;
}

ws_bme_status_t ws_bme280_bootstrap(ws_bme280_t *dev)
{
    uint8_t id;
    uint8_t status = 0u;
    uint8_t block_88[26];
    uint8_t block_e1[7];
    uint8_t attempt;

    ws_bme280_init(dev);
    cs_high();
    id = read_reg(BME_REG_ID);
    if (id != BME_CHIP_ID) {
        return WS_BME_ERR_ID;
    }
    write_reg(BME_REG_RESET, BME_SOFT_RESET);
    for (attempt = 0; attempt < 5u; attempt++) {
        ws_delay_ms(2);
        status = read_reg(BME_REG_STATUS);
        if ((status & BME_STATUS_IM_UPDATE) == 0u) {
            break;
        }
    }
    if ((status & BME_STATUS_IM_UPDATE) != 0u) {
        return WS_BME_ERR_TIMEOUT;
    }
    read_regs(BME_REG_CALIB, block_88, 26u);
    read_regs(BME_REG_CALIB_H, block_e1, 7u);
    ws_bme_parse_calibration(&dev->cal, block_88, block_e1);
    /* ctrl_hum is latched only by a later write to ctrl_meas. */
    write_reg(BME_REG_CTRL_HUM, BME_CTRL_HUM);
    write_reg(BME_REG_CONFIG, BME_CONFIG_WEATHER);
    write_reg(BME_REG_CTRL_MEAS, BME_CTRL_SLEEP);
    dev->ready = true;
    return WS_BME_OK;
}

void ws_bme280_request(ws_bme280_t *dev)
{
    if (!dev->ready || dev->awaiting) {
        return;
    }
    write_reg(BME_REG_CTRL_MEAS, BME_CTRL_FORCED);
    dev->request_ms = ws_millis();
    dev->awaiting = true;
}

static bool reading_in_range(const ws_reading_t *reading)
{
    if (reading->temperature_c_x100 < -4000 || reading->temperature_c_x100 > 8500) {
        return false;
    }
    if (reading->pressure_pa < 30000u || reading->pressure_pa > 110000u) {
        return false;
    }
    return true;
}

ws_bme_status_t ws_bme280_poll(ws_bme280_t *dev, ws_reading_t *out)
{
    uint8_t raw[8];
    int32_t adc_p;
    int32_t adc_t;
    int32_t adc_h;
    int32_t temp_x100 = 0;
    uint32_t pressure_q = 0u;
    uint32_t humidity_q = 0u;
    uint8_t status;

    if (out != NULL) {
        out->temperature_c_x100 = 0;
        out->pressure_pa = 0u;
        out->humidity_rh_x100 = 0u;
        out->valid = false;
    }
    if (!dev->ready) {
        return WS_BME_ERR_ID;
    }
    if (!dev->awaiting) {
        return WS_BME_BUSY;
    }
    status = read_reg(BME_REG_STATUS);
    if ((status & BME_STATUS_MEASURING) != 0u) {
        if ((uint32_t)(ws_millis() - dev->request_ms) >= BME_CONV_TIMEOUT_MS) {
            dev->awaiting = false;
            return WS_BME_ERR_TIMEOUT;
        }
        return WS_BME_BUSY;
    }
    read_regs(BME_REG_DATA, raw, 8u);
    dev->awaiting = false;
    adc_p = (int32_t)(((uint32_t)raw[0] << 12) | ((uint32_t)raw[1] << 4) | ((uint32_t)raw[2] >> 4));
    adc_t = (int32_t)(((uint32_t)raw[3] << 12) | ((uint32_t)raw[4] << 4) | ((uint32_t)raw[5] >> 4));
    adc_h = (int32_t)(((uint32_t)raw[6] << 8) | (uint32_t)raw[7]);
    ws_bme_compensate(&dev->cal, adc_t, adc_p, adc_h, &temp_x100, &pressure_q, &humidity_q);
    if (out == NULL) {
        return WS_BME_OK;
    }
    out->temperature_c_x100 = temp_x100;
    out->pressure_pa = ws_bme_pa_from_q24_8(pressure_q);
    out->humidity_rh_x100 = ws_bme_rh_x100_from_q22_10(humidity_q);
    out->valid = reading_in_range(out);
    if (!out->valid) {
        return WS_BME_ERR_RANGE;
    }
    return WS_BME_OK;
}
