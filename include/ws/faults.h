#pragma once

#include <stdint.h>

/* Latched in the station fault word and printed as four hex digits. */
enum {
    WS_FAULT_BME_ID = 1u << 0,       /* SPI identity was not a BME280 */
    WS_FAULT_BME_TIMEOUT = 1u << 1,  /* conversion or NVM copy did not finish */
    WS_FAULT_BME_RANGE = 1u << 2,    /* compensated value outside the operating range */
    WS_FAULT_BME_STALE = 1u << 3,    /* no good sample for three periods */
    WS_FAULT_CONFIG = 1u << 4,       /* EEPROM image failed its CRC */
    WS_FAULT_WDT_RESET = 1u << 5,    /* this boot was caused by the watchdog */
    WS_FAULT_BROWNOUT = 1u << 6      /* this boot was caused by brown-out */
};
