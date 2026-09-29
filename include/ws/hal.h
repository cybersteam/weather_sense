#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Pins match an Arduino Uno with a 16x2 keypad shield stacked on it and a
 * BME280 breakout on the SPI header. Levels are electrical: true is high.
 *
 *   LCD RS   D8   PB0
 *   LCD E    D9   PB1
 *   LCD D4   D4   PD4
 *   LCD D5   D5   PD5
 *   LCD D6   D6   PD6
 *   LCD D7   D7   PD7
 *   BME CS   D10  PB2
 *   MOSI     D11  PB3   (owned by the SPI peripheral)
 *   MISO     D12  PB4
 *   SCK      D13  PB5
 *   keypad   A0   PC0   (ADC, not a GPIO in this HAL)
 *   service  D0/D1 PD0/PD1
 */
typedef enum {
    WS_PIN_LCD_RS = 0,
    WS_PIN_LCD_E,
    WS_PIN_LCD_D4,
    WS_PIN_LCD_D5,
    WS_PIN_LCD_D6,
    WS_PIN_LCD_D7,
    WS_PIN_BME_CS,
    WS_PIN_COUNT
} ws_pin_t;

typedef enum {
    WS_RESET_POWER_ON = 0,
    WS_RESET_EXTERNAL,
    WS_RESET_BROWNOUT,
    WS_RESET_WATCHDOG,
    WS_RESET_UNKNOWN
} ws_reset_reason_t;

void ws_hal_init(void);
void ws_gpio_set(ws_pin_t pin, bool level);

uint32_t ws_millis(void);
void ws_delay_us(uint16_t us);
void ws_delay_ms(uint16_t ms);

/* SPI mode 0. Chip-select is the caller's responsibility. */
uint8_t ws_spi_xfer(uint8_t tx);

void ws_uart_write(const uint8_t *data, size_t len);
size_t ws_uart_read(uint8_t *data, size_t max_len);
void ws_uart_puts(const char *text);
/* `flash_text` is a RAM string on the host and a PROGMEM string on the AVR. */
void ws_uart_puts_P(const char *flash_text);
uint16_t ws_uart_tx_drops(void);

/* Single conversion of ADC0, 0..1023, AVCC reference. */
uint16_t ws_adc_read(void);

uint8_t ws_eeprom_read(uint16_t addr);
void ws_eeprom_write(uint16_t addr, uint8_t value);

ws_reset_reason_t ws_reset_reason(void);
void ws_wdt_kick(void);
void ws_system_reset(void);

/*
 * One HD44780 nibble, including the enable pulse. RS is the register select
 * line. The panel's R/W pin is tied low by the keypad shield, so the driver
 * waits instead of polling the busy flag.
 */
void ws_panel_write(bool rs, uint8_t nibble);
