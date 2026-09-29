#include "ws/hal.h"

void ws_delay_ms(uint16_t ms)
{
    while (ms > 0u) {
        ws_delay_us(1000);
        ws_wdt_kick();
        ms--;
    }
}
