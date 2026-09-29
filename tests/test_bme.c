#include "harness.h"
#include "fake_bme.h"

#include "ws/bme280.h"
#include "ws/hal.h"
#include "ws/hal_host.h"

#include <string.h>

void test_bme(void);

static void load_example(ws_bme_cal_t *cal)
{
    uint8_t block[26];
    uint8_t hum[7];

    memset(block, 0, sizeof block);
    memset(hum, 0, sizeof hum);
    block[0] = 0x70u;
    block[1] = 0x6Bu;
    block[2] = 0x43u;
    block[3] = 0x67u;
    block[4] = 0x18u;
    block[5] = 0xFCu;
    block[6] = 0x7Du;
    block[7] = 0x8Eu;
    block[8] = 0x43u;
    block[9] = 0xD6u;
    block[10] = 0xD0u;
    block[11] = 0x0Bu;
    block[12] = 0x27u;
    block[13] = 0x0Bu;
    block[14] = 0x8Cu;
    block[15] = 0x00u;
    block[16] = 0xF9u;
    block[17] = 0xFFu;
    block[18] = 0x8Cu;
    block[19] = 0x3Cu;
    block[20] = 0xF8u;
    block[21] = 0xC6u;
    block[22] = 0x70u;
    block[23] = 0x17u;
    block[25] = 75u;
    hum[0] = 0x52u;
    hum[1] = 0x01u;
    hum[2] = 0u;
    hum[3] = 0x14u;
    hum[4] = 0x2Eu;
    hum[5] = 0x03u;
    hum[6] = 30u;
    ws_bme_parse_calibration(cal, block, hum);
}

static double oracle_t(const ws_bme_cal_t *cal, int32_t adc, double *t_fine)
{
    double var1 = ((double)adc / 16384.0 - (double)cal->dig_T1 / 1024.0) * (double)cal->dig_T2;
    double var2 = ((double)adc / 131072.0 - (double)cal->dig_T1 / 8192.0);
    var2 = var2 * var2 * (double)cal->dig_T3;
    *t_fine = var1 + var2;
    return (var1 + var2) / 5120.0;
}

static double oracle_p(const ws_bme_cal_t *cal, int32_t adc, double t_fine)
{
    double var1 = (t_fine / 2.0) - 64000.0;
    double var2 = var1 * var1 * (double)cal->dig_P6 / 32768.0;
    double p;
    var2 = var2 + var1 * (double)cal->dig_P5 * 2.0;
    var2 = (var2 / 4.0) + ((double)cal->dig_P4 * 65536.0);
    var1 = (((double)cal->dig_P3) * var1 * var1 / 524288.0 + (double)cal->dig_P2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * (double)cal->dig_P1;
    if (var1 == 0.0) {
        return 0.0;
    }
    p = 1048576.0 - (double)adc;
    p = (p - (var2 / 4096.0)) * 6250.0 / var1;
    var1 = (double)cal->dig_P9 * p * p / 2147483648.0;
    var2 = p * (double)cal->dig_P8 / 32768.0;
    p = p + (var1 + var2 + (double)cal->dig_P7) / 16.0;
    return p;
}

static double oracle_h(const ws_bme_cal_t *cal, int32_t adc, double t_fine)
{
    double var_h = t_fine - 76800.0;
    var_h = (adc - ((double)cal->dig_H4 * 64.0 + (double)cal->dig_H5 / 16384.0 * var_h)) *
            ((double)cal->dig_H2 / 65536.0 *
             (1.0 + (double)cal->dig_H6 / 67108864.0 * var_h *
                        (1.0 + (double)cal->dig_H3 / 67108864.0 * var_h)));
    var_h = var_h * (1.0 - (double)cal->dig_H1 * var_h / 524288.0);
    if (var_h > 100.0) {
        var_h = 100.0;
    }
    if (var_h < 0.0) {
        var_h = 0.0;
    }
    return var_h;
}

static void expect_close(const ws_bme_cal_t *seed, int32_t adc_t, int32_t adc_p, int32_t adc_h)
{
    ws_bme_cal_t cal = *seed;
    int32_t temp = 0;
    uint32_t pq = 0;
    uint32_t hq = 0;
    double t_fine = 0.0;
    double t_c;
    double p_pa;
    double h_rh;
    int32_t temp_delta;
    int32_t p_delta;
    int32_t h_delta;

    t_c = oracle_t(seed, adc_t, &t_fine);
    p_pa = oracle_p(seed, adc_p, t_fine);
    h_rh = oracle_h(seed, adc_h, t_fine);
    ws_bme_compensate(&cal, adc_t, adc_p, adc_h, &temp, &pq, &hq);
    temp_delta = temp - (int32_t)(t_c * 100.0 + (t_c >= 0 ? 0.5 : -0.5));
    if (temp_delta < 0) {
        temp_delta = -temp_delta;
    }
    p_delta = (int32_t)ws_bme_pa_from_q24_8(pq) - (int32_t)(p_pa + (p_pa >= 0 ? 0.5 : -0.5));
    if (p_delta < 0) {
        p_delta = -p_delta;
    }
    h_delta = (int32_t)ws_bme_rh_x100_from_q22_10(hq) - (int32_t)(h_rh * 100.0 + 0.5);
    if (h_delta < 0) {
        h_delta = -h_delta;
    }
    EXPECT(temp_delta <= 1);
    EXPECT(p_delta <= 3);
    EXPECT(h_delta <= 5);
}

void test_bme(void)
{
    ws_bme_cal_t cal;
    fake_bme_t fake;
    ws_bme280_t dev;
    ws_reading_t reading;
    int32_t temp = 0;
    uint32_t pq = 0;
    uint32_t hq = 0;
    uint8_t neg_h[7];

    load_example(&cal);
    EXPECT_EQ(cal.dig_T1, 27504u);
    EXPECT_EQ(cal.dig_T2, 26435);
    EXPECT_EQ(cal.dig_T3, -1000);
    EXPECT_EQ(cal.dig_P1, 36477u);
    EXPECT_EQ(cal.dig_P2, -10685);
    EXPECT_EQ(cal.dig_P6, -7);
    EXPECT_EQ(cal.dig_P8, -14600);
    EXPECT_EQ(cal.dig_H1, 75);
    EXPECT_EQ(cal.dig_H2, 338);
    EXPECT_EQ(cal.dig_H4, 334);
    EXPECT_EQ(cal.dig_H5, 50);
    EXPECT_EQ(cal.dig_H6, 30);

    memset(neg_h, 0, sizeof neg_h);
    neg_h[3] = 0xF0u;
    neg_h[4] = 0x0Fu;
    neg_h[5] = 0x00u;
    {
        uint8_t block[26];
        ws_bme_cal_t signed_h;
        memset(block, 0, sizeof block);
        ws_bme_parse_calibration(&signed_h, block, neg_h);
        EXPECT_EQ(signed_h.dig_H4, -241);
    }

    ws_bme_compensate(&cal, 519888, 415148, 30000, &temp, &pq, &hq);
    EXPECT_EQ(temp, 2508);
    EXPECT_NEAR((int32_t)ws_bme_pa_from_q24_8(pq), 100653, 2);

    expect_close(&cal, 519888, 415148, 30000);
    expect_close(&cal, 400000, 300000, 25000);
    expect_close(&cal, 600000, 500000, 36000);
    expect_close(&cal, 200000, 350000, 22000);

    ws_host_reset();
    fake_bme_init(&fake);
    fake_bme_attach(&fake);
    ws_hal_init();
    EXPECT_EQ(ws_bme280_bootstrap(&dev), WS_BME_OK);
    EXPECT(dev.ready);
    ws_bme280_request(&dev);
    EXPECT_EQ(ws_bme280_poll(&dev, &reading), WS_BME_OK);
    EXPECT(reading.valid);
    EXPECT_EQ(reading.temperature_c_x100, 2508);
    EXPECT_NEAR((int32_t)reading.pressure_pa, 100653, 2);
    EXPECT(reading.humidity_rh_x100 > 4000u && reading.humidity_rh_x100 < 5000u);

    fake_bme_set_id(&fake, 0x00u);
    EXPECT_EQ(ws_bme280_bootstrap(&dev), WS_BME_ERR_ID);
    EXPECT(!dev.ready);

    fake_bme_init(&fake);
    fake_bme_stick_im_update(&fake, true);
    fake_bme_attach(&fake);
    EXPECT_EQ(ws_bme280_bootstrap(&dev), WS_BME_ERR_TIMEOUT);

    fake_bme_init(&fake);
    fake_bme_hold(&fake, true);
    fake_bme_attach(&fake);
    EXPECT_EQ(ws_bme280_bootstrap(&dev), WS_BME_OK);
    ws_bme280_request(&dev);
    EXPECT_EQ(ws_bme280_poll(&dev, &reading), WS_BME_BUSY);
    ws_host_advance_ms(30);
    EXPECT_EQ(ws_bme280_poll(&dev, &reading), WS_BME_ERR_TIMEOUT);
}
