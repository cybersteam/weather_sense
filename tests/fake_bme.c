#include "fake_bme.h"

#include "ws/hal_host.h"

#include <string.h>

static void put_u16(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void put_i16(uint8_t *dst, int16_t value)
{
    put_u16(dst, (uint16_t)value);
}

static void load_example(fake_bme_t *fake)
{
    uint8_t *r = fake->regs;

    memset(r, 0, 256);
    fake->id = 0x60u;
    r[0xD0] = fake->id;
    put_u16(&r[0x88], 27504u);
    put_i16(&r[0x8A], 26435);
    put_i16(&r[0x8C], -1000);
    put_u16(&r[0x8E], 36477u);
    put_i16(&r[0x90], -10685);
    put_i16(&r[0x92], 3024);
    put_i16(&r[0x94], 2855);
    put_i16(&r[0x96], 140);
    put_i16(&r[0x98], -7);
    put_i16(&r[0x9A], 15500);
    put_i16(&r[0x9C], -14600);
    put_i16(&r[0x9E], 6000);
    r[0xA1] = 75u;
    put_i16(&r[0xE1], 338);
    r[0xE3] = 0u;
    /* H4 = 334, H5 = 50, both 12-bit. */
    r[0xE4] = 0x14u;
    r[0xE5] = 0x2Eu;
    r[0xE6] = 0x03u;
    r[0xE7] = 30u;
    /* adc_P 415148, adc_T 519888, adc_H 30000. */
    r[0xF7] = 0x65u;
    r[0xF8] = 0x5Au;
    r[0xF9] = 0xC0u;
    r[0xFA] = 0x7Eu;
    r[0xFB] = 0xEDu;
    r[0xFC] = 0x00u;
    r[0xFD] = 0x75u;
    r[0xFE] = 0x30u;
}

void fake_bme_init(fake_bme_t *fake)
{
    memset(fake, 0, sizeof *fake);
    load_example(fake);
}

void fake_bme_set_id(fake_bme_t *fake, uint8_t id)
{
    fake->id = id;
    fake->regs[0xD0] = id;
}

void fake_bme_hold(fake_bme_t *fake, bool hold)
{
    fake->hold = hold;
}

void fake_bme_stick_im_update(fake_bme_t *fake, bool stick)
{
    fake->stick_im_update = stick;
}

static void on_write(fake_bme_t *fake, uint8_t addr, uint8_t value)
{
    if (addr == 0xE0u && value == 0xB6u) {
        uint8_t cal[26];
        uint8_t hum[7];
        uint8_t data[8];
        memcpy(cal, &fake->regs[0x88], sizeof cal);
        memcpy(hum, &fake->regs[0xE1], sizeof hum);
        memcpy(data, &fake->regs[0xF7], sizeof data);
        fake->regs[0xD0] = fake->id;
        fake->regs[0xF2] = 0u;
        fake->regs[0xF3] = fake->stick_im_update ? 0x01u : 0x00u;
        fake->regs[0xF4] = 0u;
        fake->regs[0xF5] = 0u;
        memcpy(&fake->regs[0x88], cal, sizeof cal);
        memcpy(&fake->regs[0xE1], hum, sizeof hum);
        memcpy(&fake->regs[0xF7], data, sizeof data);
        return;
    }
    if (addr == 0xF4u) {
        uint8_t mode = (uint8_t)(value & 0x03u);
        if (mode == 0x01u || mode == 0x02u) {
            if (fake->hold) {
                fake->regs[0xF3] = 0x08u;
            } else {
                fake->regs[0xF3] = 0x00u;
                fake->regs[0xF4] = (uint8_t)(value & (uint8_t)~0x03u);
            }
        }
    }
}

static uint8_t fake_spi(uint8_t tx, void *ctx)
{
    fake_bme_t *fake = ctx;
    uint32_t stamp = ws_host_gpio_stamp();

    if (stamp != fake->seen_stamp) {
        fake->seen_stamp = stamp;
        fake->phase = 0u;
    }
    if (ws_host_pin(WS_PIN_BME_CS)) {
        fake->phase = 0u;
        return 0xFFu;
    }
    if (fake->phase == 0u) {
        /* Every BME280 register is documented in 0x80..0xFF. SPI uses bit 7
         * as the read flag, so a write command is that address with bit 7
         * cleared and a read command is the address itself. */
        fake->is_read = (tx & 0x80u) != 0u;
        fake->addr = fake->is_read ? tx : (uint8_t)(tx | 0x80u);
        fake->phase = 1u;
        return 0x00u;
    }
    if (fake->is_read) {
        uint8_t value = fake->regs[fake->addr];
        fake->addr++;
        return value;
    }
    fake->regs[fake->addr] = tx;
    on_write(fake, fake->addr, tx);
    return 0x00u;
}

void fake_bme_attach(fake_bme_t *fake)
{
    fake->seen_stamp = ws_host_gpio_stamp();
    fake->phase = 0u;
    ws_host_set_spi(fake_spi, fake);
}
