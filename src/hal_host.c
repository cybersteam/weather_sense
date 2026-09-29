#include "ws/hal.h"

#include "ws/hal_host.h"

#include <string.h>

#define HOST_EEPROM_BYTES 1024u
#define HOST_TX_BYTES 8192u
#define HOST_RX_BYTES 256u
#define HOST_PANEL_EVENTS 512u

static uint8_t eeprom[HOST_EEPROM_BYTES];
static uint32_t eeprom_writes;
static uint32_t millis_now;
static uint8_t pin_level[WS_PIN_COUNT];
static uint32_t gpio_stamp;
static uint16_t adc_value;
static ws_reset_reason_t reset_reason;
static bool reset_requested;
static uint16_t tx_drops;

static char tx[HOST_TX_BYTES];
static size_t tx_len;
static uint8_t rx[HOST_RX_BYTES];
static size_t rx_head;
static size_t rx_tail;

static uint8_t (*spi_fn)(uint8_t tx_byte, void *ctx);
static void *spi_ctx;

typedef struct {
    uint8_t rs;
    uint8_t nibble;
} panel_event_t;

static panel_event_t panel[HOST_PANEL_EVENTS];
static size_t panel_count;

void ws_host_reset(void)
{
    memset(eeprom, 0xFF, sizeof eeprom);
    eeprom_writes = 0u;
    millis_now = 0u;
    memset(pin_level, 0, sizeof pin_level);
    gpio_stamp = 0u;
    adc_value = 1023u;
    reset_reason = WS_RESET_POWER_ON;
    reset_requested = false;
    tx_drops = 0u;
    tx_len = 0u;
    tx[0] = '\0';
    rx_head = 0u;
    rx_tail = 0u;
    spi_fn = NULL;
    spi_ctx = NULL;
    panel_count = 0u;
    pin_level[WS_PIN_BME_CS] = 1u;
}

void ws_host_reboot(void)
{
    millis_now = 0u;
    tx_len = 0u;
    tx[0] = '\0';
    rx_head = 0u;
    rx_tail = 0u;
    reset_requested = false;
    panel_count = 0u;
    memset(pin_level, 0, sizeof pin_level);
    pin_level[WS_PIN_BME_CS] = 1u;
    gpio_stamp++;
}

void ws_host_advance_ms(uint32_t ms)
{
    millis_now += ms;
}

void ws_host_set_reset_reason(ws_reset_reason_t reason)
{
    reset_reason = reason;
}

void ws_host_set_adc(uint16_t adc)
{
    adc_value = adc;
}

void ws_host_rx(const char *text)
{
    while (text != NULL && *text != '\0') {
        size_t next = (rx_head + 1u) % HOST_RX_BYTES;
        if (next == rx_tail) {
            break;
        }
        rx[rx_head] = (uint8_t)*text++;
        rx_head = next;
    }
}

const char *ws_host_tx(void)
{
    return tx;
}

void ws_host_tx_clear(void)
{
    tx_len = 0u;
    tx[0] = '\0';
}

uint32_t ws_host_eeprom_writes(void)
{
    return eeprom_writes;
}

uint8_t ws_host_eeprom(uint16_t addr)
{
    if (addr >= HOST_EEPROM_BYTES) {
        return 0xFFu;
    }
    return eeprom[addr];
}

void ws_host_set_spi(uint8_t (*fn)(uint8_t tx_byte, void *ctx), void *ctx)
{
    spi_fn = fn;
    spi_ctx = ctx;
}

uint32_t ws_host_gpio_stamp(void)
{
    return gpio_stamp;
}

bool ws_host_pin(ws_pin_t pin)
{
    if ((unsigned)pin >= WS_PIN_COUNT) {
        return false;
    }
    return pin_level[pin] != 0u;
}

bool ws_host_reset_requested(void)
{
    return reset_requested;
}

size_t ws_host_panel_count(void)
{
    return panel_count;
}

void ws_host_panel_at(size_t index, bool *rs, uint8_t *nibble)
{
    if (index >= panel_count) {
        *rs = false;
        *nibble = 0u;
        return;
    }
    *rs = panel[index].rs != 0u;
    *nibble = panel[index].nibble;
}

void ws_host_panel_clear(void)
{
    panel_count = 0u;
}

void ws_hal_init(void)
{
    pin_level[WS_PIN_BME_CS] = 1u;
    gpio_stamp++;
}

void ws_gpio_set(ws_pin_t pin, bool level)
{
    if ((unsigned)pin >= WS_PIN_COUNT) {
        return;
    }
    pin_level[pin] = level ? 1u : 0u;
    gpio_stamp++;
}

uint32_t ws_millis(void)
{
    return millis_now;
}

void ws_delay_us(uint16_t us)
{
    (void)us;
}

uint8_t ws_spi_xfer(uint8_t byte)
{
    if (spi_fn == NULL) {
        return 0xFFu;
    }
    return spi_fn(byte, spi_ctx);
}

void ws_uart_write(const uint8_t *data, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        if (tx_len + 1u >= HOST_TX_BYTES) {
            tx_drops++;
            break;
        }
        tx[tx_len++] = (char)data[i];
    }
    tx[tx_len] = '\0';
}

size_t ws_uart_read(uint8_t *data, size_t max_len)
{
    size_t n = 0;

    while (n < max_len && rx_head != rx_tail) {
        data[n++] = rx[rx_tail];
        rx_tail = (rx_tail + 1u) % HOST_RX_BYTES;
    }
    return n;
}

uint16_t ws_uart_tx_drops(void)
{
    return tx_drops;
}

uint16_t ws_adc_read(void)
{
    return adc_value;
}

uint8_t ws_eeprom_read(uint16_t addr)
{
    if (addr >= HOST_EEPROM_BYTES) {
        return 0xFFu;
    }
    return eeprom[addr];
}

void ws_eeprom_write(uint16_t addr, uint8_t value)
{
    if (addr >= HOST_EEPROM_BYTES) {
        return;
    }
    if (eeprom[addr] == value) {
        return;
    }
    eeprom[addr] = value;
    eeprom_writes++;
}

ws_reset_reason_t ws_reset_reason(void)
{
    return reset_reason;
}

void ws_wdt_kick(void)
{
}

void ws_system_reset(void)
{
    reset_requested = true;
}

void ws_panel_write(bool rs, uint8_t nibble)
{
    if (panel_count >= HOST_PANEL_EVENTS) {
        return;
    }
    panel[panel_count].rs = rs ? 1u : 0u;
    panel[panel_count].nibble = (uint8_t)(nibble & 0x0Fu);
    panel_count++;
}
