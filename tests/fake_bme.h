#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t regs[256];
    uint8_t id;
    bool hold;
    bool stick_im_update;
    uint32_t seen_stamp;
    uint8_t phase;
    uint8_t addr;
    bool is_read;
} fake_bme_t;

void fake_bme_init(fake_bme_t *fake);
void fake_bme_attach(fake_bme_t *fake);
void fake_bme_set_id(fake_bme_t *fake, uint8_t id);
void fake_bme_hold(fake_bme_t *fake, bool hold);
void fake_bme_stick_im_update(fake_bme_t *fake, bool stick);
