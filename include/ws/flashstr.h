#pragma once

/*
 * Constant strings are PROGMEM on the AVR (string literals otherwise land in
 * SRAM) and ordinary literals in the host test build.
 */
#ifdef WS_HOST
#define WS_PSTR(s) ((const char *)(s))
static inline char ws_flash_byte(const char *p)
{
    return *p;
}
#else
#include <avr/pgmspace.h>
#define WS_PSTR(s) ((const char *)PSTR(s))
static inline char ws_flash_byte(const char *p)
{
    return (char)pgm_read_byte(p);
}
#endif
