#include "ws/metrics.h"

#include <stddef.h>

/* Q16.16. 1.0 == 65536. ln(2) and the Magnus constants are rounded once. */
#define Q16_ONE 65536L
#define LN2_Q16 45426L
#define MAGNUS_A_Q16 1155072L  /* 17.625, exact */
#define MAGNUS_B_Q16 15927869L /* 243.04 */
#define HYPSO_K_Q16 2239L      /* 0.0341632 K/m */

static int32_t mul_q16(int32_t a, int32_t b)
{
    return (int32_t)(((int64_t)a * (int64_t)b) >> 16);
}

static int32_t div_q16(int32_t a, int32_t b)
{
    if (b == 0) {
        return 0;
    }
    return (int32_t)(((int64_t)a * (int64_t)Q16_ONE) / b);
}

/* Natural log of a positive Q16.16 value. Series on (m-1)/(m+1), m in [1, 2). */
static int32_t ln_q16(int32_t x)
{
    int32_t k = 0;
    int32_t num;
    int32_t den;
    int32_t z;
    int32_t z2;
    int32_t term;
    int32_t sum;
    int n;

    if (x <= 0) {
        return -20 * Q16_ONE;
    }
    while (x >= (2 * Q16_ONE) && k < 16) {
        x >>= 1;
        k++;
    }
    while (x < Q16_ONE && k > -16) {
        x <<= 1;
        k--;
    }
    num = x - Q16_ONE;
    den = x + Q16_ONE;
    z = div_q16(num, den);
    z2 = mul_q16(z, z);
    term = z;
    sum = z;
    for (n = 3; n <= 17; n += 2) {
        term = mul_q16(term, z2);
        sum += term / n;
    }
    return (sum << 1) + k * LN2_Q16;
}

static int32_t exp_q16(int32_t x)
{
    int32_t term = Q16_ONE;
    int32_t sum = Q16_ONE;
    int n;

    if (x > 50000) {
        x = 50000;
    }
    if (x < -20000) {
        x = -20000;
    }
    for (n = 1; n <= 14; n++) {
        term = (int32_t)(((int64_t)term * x) / ((int64_t)n * Q16_ONE));
        sum += term;
    }
    if (sum < 0) {
        return 0;
    }
    return sum;
}

static int16_t clamp_i16(int32_t value, int32_t lo, int32_t hi)
{
    if (value < lo) {
        return (int16_t)lo;
    }
    if (value > hi) {
        return (int16_t)hi;
    }
    return (int16_t)value;
}

int16_t ws_dew_point_c_x100(int16_t temp_c_x100, uint16_t rh_x100)
{
    int32_t t_q;
    int32_t ln_rh;
    int32_t gamma;
    int32_t td_q;
    int32_t td_x100;
    int32_t ratio;

    if (rh_x100 == 0u) {
        return -8000;
    }
    if (rh_x100 > 10000u) {
        rh_x100 = 10000u;
    }
    t_q = (int32_t)(((int64_t)temp_c_x100 * Q16_ONE) / 100);
    ratio = (int32_t)(((int64_t)rh_x100 << 16) / 10000);
    ln_rh = ln_q16(ratio);
    gamma = ln_rh + div_q16(mul_q16(MAGNUS_A_Q16, t_q), MAGNUS_B_Q16 + t_q);
    if (MAGNUS_A_Q16 - gamma == 0) {
        return temp_c_x100;
    }
    td_q = div_q16(mul_q16(MAGNUS_B_Q16, gamma), MAGNUS_A_Q16 - gamma);
    td_x100 = (int32_t)(((int64_t)td_q * 100) / Q16_ONE);
    return clamp_i16(td_x100, -8000, 8500);
}

int32_t ws_c_to_f_x100(int32_t temp_c_x100)
{
    return (temp_c_x100 * 9) / 5 + 3200;
}

int32_t ws_f_to_c_x100(int32_t temp_f_x100)
{
    return ((temp_f_x100 - 3200) * 5) / 9;
}

static uint32_t isqrt_u64(uint64_t n)
{
    uint64_t x;
    uint64_t y;

    if (n == 0u) {
        return 0u;
    }
    x = n;
    y = (x + 1u) / 2u;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2u;
    }
    return (uint32_t)x;
}

/* coef * T^a * R^b, returned in hundredths of a degree Fahrenheit. */
static int32_t hi_term(int64_t coef_e8, int64_t powers, int64_t denom)
{
    return (int32_t)((coef_e8 * powers) / denom);
}

int16_t ws_heat_index_f_x100(int16_t temp_f_x100, uint16_t rh_x100, bool *applicable)
{
    int32_t t = temp_f_x100;
    int32_t r = (int32_t)rh_x100;
    int32_t part_t = ((t - 6800) * 12) / 10;
    int32_t part_r = (int32_t)(((int64_t)r * 94) / 1000);
    int32_t simple = (t + 6100 + part_t + part_r) / 2;
    int32_t avg = (simple + t) / 2;
    int64_t t2;
    int64_t r2;
    int32_t hi;
    bool full;

    if (r < 0) {
        r = 0;
    }
    if (r > 10000) {
        r = 10000;
    }
    full = avg >= 8000;
    if (applicable != NULL) {
        *applicable = full;
    }
    if (!full) {
        return clamp_i16(simple, -8000, 20000);
    }

    t2 = (int64_t)t * t;
    r2 = (int64_t)r * r;
    hi = -4238;
    hi += hi_term(204901523LL, t, 100000000LL);
    hi += hi_term(1014333127LL, r, 100000000LL);
    hi += hi_term(-22475541LL, (int64_t)t * r, 10000000000LL);
    hi += hi_term(-683783LL, t2, 10000000000LL);
    hi += hi_term(-5481717LL, r2, 10000000000LL);
    hi += hi_term(122874LL, t2 * r, 1000000000000LL);
    hi += hi_term(85282LL, (int64_t)t * r2, 1000000000000LL);
    hi += hi_term(-199LL, t2 * r2, 100000000000000LL);

    if (r < 1300 && t >= 8000 && t <= 11200) {
        int32_t abs_dt = (t >= 9500) ? (t - 9500) : (9500 - t);
        uint32_t numer;
        uint32_t ratio_q16;
        uint32_t root;
        int32_t lead;
        int32_t adj;

        if (abs_dt > 1700) {
            abs_dt = 1700;
        }
        numer = (uint32_t)(1700 - abs_dt);
        ratio_q16 = (numer * 65536u) / 1700u;
        root = isqrt_u64((uint64_t)ratio_q16 << 16);
        lead = (1300 - r) / 4;
        adj = (int32_t)(((int64_t)lead * (int64_t)root) / 65536);
        hi -= adj;
    }
    if (r > 8500 && t >= 8000 && t <= 8700) {
        hi += (int32_t)(((int64_t)(r - 8500) * (8700 - t)) / 5000);
    }
    return clamp_i16(hi, -8000, 20000);
}

int16_t ws_heat_index_c_x100(int16_t temp_c_x100, uint16_t rh_x100, bool *applicable)
{
    int16_t hi_f = ws_heat_index_f_x100((int16_t)ws_c_to_f_x100(temp_c_x100),
                                        rh_x100, applicable);
    return (int16_t)ws_f_to_c_x100(hi_f);
}

uint32_t ws_sea_level_pa(uint32_t station_pa, int16_t temp_c_x100, int16_t altitude_m)
{
    int32_t tk_x100;
    int32_t exponent;
    int32_t factor;

    if (altitude_m == 0 || station_pa == 0u) {
        return station_pa;
    }
    tk_x100 = (int32_t)temp_c_x100 + 27315;
    if (tk_x100 < 17315) {
        tk_x100 = 17315;
    }
    exponent = (int32_t)(((int64_t)HYPSO_K_Q16 * altitude_m * 100) / tk_x100);
    factor = exp_q16(exponent);
    return (uint32_t)(((uint64_t)station_pa * (uint32_t)factor + 32768u) >> 16);
}

void ws_tendency_init(ws_tendency_t *tend)
{
    uint8_t i;

    for (i = 0; i < WS_TEND_POINTS; i++) {
        tend->hpa_x10[i] = 0u;
    }
    tend->count = 0u;
    tend->next = 0u;
    tend->last_record_ms = 0u;
    tend->started = false;
    tend->current = WS_TEND_UNKNOWN;
}

static int8_t tendency_from_buffer(const ws_tendency_t *tend)
{
    uint16_t oldest;
    uint16_t newest;
    int32_t delta;

    if (tend->count < WS_TEND_POINTS) {
        return WS_TEND_UNKNOWN;
    }
    oldest = tend->hpa_x10[tend->next];
    newest = tend->hpa_x10[(uint8_t)((tend->next + WS_TEND_POINTS - 1u) % WS_TEND_POINTS)];
    delta = (int32_t)newest - (int32_t)oldest;
    if (delta >= WS_TEND_THRESHOLD_HPA_X10) {
        return WS_TEND_RISING;
    }
    if (delta <= -WS_TEND_THRESHOLD_HPA_X10) {
        return WS_TEND_FALLING;
    }
    return WS_TEND_STEADY;
}

int8_t ws_tendency_update(ws_tendency_t *tend, uint32_t pressure_pa, uint32_t now_ms)
{
    uint16_t sample;

    if (tend->started && (uint32_t)(now_ms - tend->last_record_ms) < WS_TEND_INTERVAL_MS) {
        return tend->current;
    }
    if (pressure_pa < 30000u) {
        sample = 3000u;
    } else if (pressure_pa > 110000u) {
        sample = 11000u;
    } else {
        sample = (uint16_t)((pressure_pa + 5u) / 10u);
    }
    tend->hpa_x10[tend->next] = sample;
    tend->next = (uint8_t)((tend->next + 1u) % WS_TEND_POINTS);
    if (tend->count < WS_TEND_POINTS) {
        tend->count++;
    }
    tend->last_record_ms = now_ms;
    tend->started = true;
    tend->current = tendency_from_buffer(tend);
    return tend->current;
}
