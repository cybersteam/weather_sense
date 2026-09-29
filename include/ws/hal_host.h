#pragma once

/*
 * Host-only controls for the HAL. Firmware never includes this header.
 * Time does not advance on its own: tests call ws_host_advance_ms.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ws/hal.h"

void ws_host_reset(void);
void ws_host_reboot(void);
void ws_host_advance_ms(uint32_t ms);
void ws_host_set_reset_reason(ws_reset_reason_t reason);
void ws_host_set_adc(uint16_t adc);

void ws_host_rx(const char *text);
const char *ws_host_tx(void);
void ws_host_tx_clear(void);
uint32_t ws_host_eeprom_writes(void);
uint8_t ws_host_eeprom(uint16_t addr);

void ws_host_set_spi(uint8_t (*fn)(uint8_t tx, void *ctx), void *ctx);
uint32_t ws_host_gpio_stamp(void);
bool ws_host_pin(ws_pin_t pin);
bool ws_host_reset_requested(void);

size_t ws_host_panel_count(void);
void ws_host_panel_at(size_t index, bool *rs, uint8_t *nibble);
void ws_host_panel_clear(void);
