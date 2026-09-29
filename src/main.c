#include "ws/hal.h"
#include "ws/station.h"

int main(void)
{
    ws_hal_init();
    ws_station_init();
    for (;;) {
        ws_wdt_kick();
        ws_station_poll();
    }
    return 0;
}
