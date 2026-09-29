#ifndef F_CPU
#error "F_CPU must be defined as 16000000UL"
#endif

#include "ws/hal.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay_basic.h>

#define TX_SIZE 128u
#define TX_MASK (TX_SIZE - 1u)
#define RX_SIZE 64u
#define RX_MASK (RX_SIZE - 1u)

/* Survives BSS clear. Captured from MCUSR in .init3 before that clear runs. */
uint8_t ws_reset_flags __attribute__((section(".noinit")));

/*
 * .init3 runs after the stack exists and before .data/.bss setup. The body is
 * naked so it falls through into .init4 instead of returning. WDTCSR is outside
 * the I/O space on this part, so the watchdog unlock uses STS. The two stores
 * have to stay adjacent: the write-enable window is four cycles.
 */
void ws_early_init(void) __attribute__((naked, used, section(".init3")));
void ws_early_init(void)
{
    __asm__ __volatile__(
        "in r24, %[mcusr]\n\t"
        "sts ws_reset_flags, r24\n\t"
        "out %[mcusr], __zero_reg__\n\t"
        "wdr\n\t"
        "ldi r24, %[wdt_change]\n\t"
        "sts %[wdtcsr], r24\n\t"
        "sts %[wdtcsr], __zero_reg__\n\t"
        :
        : [mcusr] "I"(_SFR_IO_ADDR(MCUSR)), [wdtcsr] "n"(_SFR_MEM_ADDR(WDTCSR)),
          [wdt_change] "M"((uint8_t)((1u << WDCE) | (1u << WDE)))
        : "r24");
}

static volatile uint32_t ws_ms;
static volatile uint8_t tx_buf[TX_SIZE];
static volatile uint8_t tx_head;
static volatile uint8_t tx_tail;
static volatile uint8_t rx_buf[RX_SIZE];
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;
static volatile uint16_t tx_drops;

ISR(TIMER2_COMPA_vect)
{
    ws_ms++;
}

ISR(USART_UDRE_vect)
{
    if (tx_head == tx_tail) {
        UCSR0B = (uint8_t)(UCSR0B & (uint8_t)~(1u << UDRIE0));
        return;
    }
    UDR0 = tx_buf[tx_tail];
    tx_tail = (uint8_t)((tx_tail + 1u) & TX_MASK);
}

ISR(USART_RX_vect)
{
    uint8_t byte = UDR0;
    uint8_t next = (uint8_t)((rx_head + 1u) & RX_MASK);

    if (next == rx_tail) {
        return;
    }
    rx_buf[rx_head] = byte;
    rx_head = next;
}

static void pin_ddr_out(volatile uint8_t *ddr, uint8_t bit)
{
    *ddr = (uint8_t)(*ddr | (uint8_t)(1u << bit));
}

static void pin_write(volatile uint8_t *port, uint8_t bit, bool level)
{
    if (level) {
        *port = (uint8_t)(*port | (uint8_t)(1u << bit));
    } else {
        *port = (uint8_t)(*port & (uint8_t)~(1u << bit));
    }
}

void ws_gpio_set(ws_pin_t pin, bool level)
{
    switch (pin) {
    case WS_PIN_LCD_RS:
        pin_write(&PORTB, PB0, level);
        break;
    case WS_PIN_LCD_E:
        pin_write(&PORTB, PB1, level);
        break;
    case WS_PIN_LCD_D4:
        pin_write(&PORTD, PD4, level);
        break;
    case WS_PIN_LCD_D5:
        pin_write(&PORTD, PD5, level);
        break;
    case WS_PIN_LCD_D6:
        pin_write(&PORTD, PD6, level);
        break;
    case WS_PIN_LCD_D7:
        pin_write(&PORTD, PD7, level);
        break;
    case WS_PIN_BME_CS:
        pin_write(&PORTB, PB2, level);
        break;
    default:
        break;
    }
}

void ws_hal_init(void)
{
    pin_ddr_out(&DDRB, PB0);
    pin_ddr_out(&DDRB, PB1);
    pin_ddr_out(&DDRB, PB2);
    pin_ddr_out(&DDRB, PB3);
    pin_ddr_out(&DDRB, PB5);
    DDRB = (uint8_t)(DDRB & (uint8_t)~(1u << PB4));
    PORTB = (uint8_t)(PORTB | (uint8_t)(1u << PB4));
    pin_ddr_out(&DDRD, PD4);
    pin_ddr_out(&DDRD, PD5);
    pin_ddr_out(&DDRD, PD6);
    pin_ddr_out(&DDRD, PD7);
    ws_gpio_set(WS_PIN_BME_CS, true);
    ws_gpio_set(WS_PIN_LCD_E, false);

    /* 1 MHz SPI, mode 0. The BME280 accepts up to 10 MHz. */
    SPCR = (uint8_t)((1u << SPE) | (1u << MSTR) | (1u << SPR0));
    SPSR = (uint8_t)(SPSR & (uint8_t)~(1u << SPI2X));

    /* 9600 8N1. UBRR = F_CPU/16/baud - 1 = 103, error about 0.2%. */
    UBRR0H = 0u;
    UBRR0L = (uint8_t)(F_CPU / 16UL / 9600UL - 1UL);
    UCSR0C = (uint8_t)((1u << UCSZ01) | (1u << UCSZ00));
    UCSR0B = (uint8_t)((1u << RXEN0) | (1u << TXEN0) | (1u << RXCIE0));

    /* AVCC reference, ADC0, prescale 128 -> 125 kHz. Discard the first sample. */
    ADMUX = (uint8_t)(1u << REFS0);
    ADCSRA = (uint8_t)((1u << ADEN) | (1u << ADPS2) | (1u << ADPS1) | (1u << ADPS0));
    ADCSRA = (uint8_t)(ADCSRA | (uint8_t)(1u << ADSC));
    while ((ADCSRA & (1u << ADSC)) != 0u) {
    }

    /* Timer2 CTC, prescale 128, OCR 124 -> 1.000 ms at 16 MHz. */
    TCCR2A = (uint8_t)(1u << WGM21);
    TCCR2B = (uint8_t)((1u << CS22) | (1u << CS20));
    OCR2A = 124u;
    TIMSK2 = (uint8_t)(1u << OCIE2A);

    wdt_enable(WDTO_2S);
    sei();
}

uint32_t ws_millis(void)
{
    uint8_t sreg = SREG;
    uint32_t now;

    cli();
    now = ws_ms;
    SREG = sreg;
    return now;
}

void ws_delay_us(uint16_t us)
{
    /* _delay_loop_2(4) is 16 cycles, 1 us at 16 MHz, plus the loop overhead. */
    while (us > 0u) {
        _delay_loop_2(4);
        us--;
    }
}

uint8_t ws_spi_xfer(uint8_t tx)
{
    SPDR = tx;
    while ((SPSR & (1u << SPIF)) == 0u) {
    }
    return SPDR;
}

void ws_uart_write(const uint8_t *data, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        uint8_t next = (uint8_t)((tx_head + 1u) & TX_MASK);
        if (next == tx_tail) {
            tx_drops++;
            break;
        }
        tx_buf[tx_head] = data[i];
        tx_head = next;
    }
    UCSR0B = (uint8_t)(UCSR0B | (uint8_t)(1u << UDRIE0));
}

size_t ws_uart_read(uint8_t *data, size_t max_len)
{
    size_t n = 0;

    while (n < max_len && rx_head != rx_tail) {
        data[n++] = rx_buf[rx_tail];
        rx_tail = (uint8_t)((rx_tail + 1u) & RX_MASK);
    }
    return n;
}

uint16_t ws_uart_tx_drops(void)
{
    return tx_drops;
}

uint16_t ws_adc_read(void)
{
    ADCSRA = (uint8_t)(ADCSRA | (uint8_t)(1u << ADSC));
    while ((ADCSRA & (1u << ADSC)) != 0u) {
    }
    return ADC;
}

uint8_t ws_eeprom_read(uint16_t addr)
{
    while ((EECR & (1u << EEPE)) != 0u) {
    }
    EEAR = addr;
    EECR = (uint8_t)(EECR | (uint8_t)(1u << EERE));
    return EEDR;
}

void ws_eeprom_write(uint16_t addr, uint8_t value)
{
    uint8_t sreg;

    if (ws_eeprom_read(addr) == value) {
        return;
    }
    while ((EECR & (1u << EEPE)) != 0u) {
    }
    EEAR = addr;
    EEDR = value;
    sreg = SREG;
    cli();
    EECR = (uint8_t)(EECR | (uint8_t)(1u << EEMPE));
    EECR = (uint8_t)(EECR | (uint8_t)(1u << EEPE));
    SREG = sreg;
}

ws_reset_reason_t ws_reset_reason(void)
{
    /* Watchdog outranks brown-out when both flags are set. */
    if ((ws_reset_flags & (1u << WDRF)) != 0u) {
        return WS_RESET_WATCHDOG;
    }
    if ((ws_reset_flags & (1u << BORF)) != 0u) {
        return WS_RESET_BROWNOUT;
    }
    if ((ws_reset_flags & (1u << EXTRF)) != 0u) {
        return WS_RESET_EXTERNAL;
    }
    if ((ws_reset_flags & (1u << PORF)) != 0u) {
        return WS_RESET_POWER_ON;
    }
    return WS_RESET_UNKNOWN;
}

void ws_wdt_kick(void)
{
    wdt_reset();
}

void ws_system_reset(void)
{
    wdt_enable(WDTO_15MS);
    for (;;) {
    }
}

void ws_panel_write(bool rs, uint8_t nibble)
{
    ws_gpio_set(WS_PIN_LCD_RS, rs);
    ws_gpio_set(WS_PIN_LCD_D4, (nibble & 0x01u) != 0u);
    ws_gpio_set(WS_PIN_LCD_D5, (nibble & 0x02u) != 0u);
    ws_gpio_set(WS_PIN_LCD_D6, (nibble & 0x04u) != 0u);
    ws_gpio_set(WS_PIN_LCD_D7, (nibble & 0x08u) != 0u);
    ws_delay_us(1);
    ws_gpio_set(WS_PIN_LCD_E, true);
    ws_delay_us(1);
    ws_gpio_set(WS_PIN_LCD_E, false);
    ws_delay_us(1);
}
