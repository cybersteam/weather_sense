#include "ws/hal.h"

#include "ws/flashstr.h"

void ws_uart_puts(const char *text)
{
    size_t n = 0;

    if (text == NULL) {
        return;
    }
    while (text[n] != '\0') {
        n++;
    }
    ws_uart_write((const uint8_t *)text, n);
}

void ws_uart_puts_P(const char *flash_text)
{
    char buf[32];
    size_t n = 0;

    if (flash_text == NULL) {
        return;
    }
    for (;;) {
        char c = ws_flash_byte(flash_text);
        flash_text++;
        if (c == '\0') {
            break;
        }
        buf[n++] = c;
        if (n == sizeof buf) {
            ws_uart_write((const uint8_t *)buf, n);
            n = 0;
        }
    }
    if (n > 0u) {
        ws_uart_write((const uint8_t *)buf, n);
    }
}
