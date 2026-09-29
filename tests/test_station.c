#include "harness.h"
#include "fake_bme.h"

#include "ws/faults.h"
#include "ws/hal.h"
#include "ws/hal_host.h"
#include "ws/lcd.h"
#include "ws/station.h"

#include <string.h>

void test_station(void);

static bool tx_has(const char *text)
{
    return strstr(ws_host_tx(), text) != NULL;
}

static void boot(fake_bme_t *fake, bool present)
{
    ws_host_reset();
    if (present) {
        fake_bme_init(fake);
        fake_bme_attach(fake);
    }
    ws_hal_init();
    ws_station_init();
}

void test_station(void)
{
    fake_bme_t fake;
    char line0[17];
    char line1[17];
    uint32_t writes;

    boot(&fake, false);
    EXPECT((ws_station_faults() & WS_FAULT_BME_ID) != 0u);
    EXPECT_EQ(ws_station_samples(), 0u);
    EXPECT(tx_has("WeatherSense 1.0.0"));
    EXPECT(tx_has("reset=POR"));
    ws_host_tx_clear();
    ws_host_rx("VER\n");
    ws_station_poll();
    EXPECT(tx_has("OK WeatherSense 1.0.0 atmega328p reset=POR"));
    EXPECT(!ws_host_reset_requested());

    boot(&fake, true);
    EXPECT((ws_station_faults() & WS_FAULT_BME_ID) == 0u);
    ws_host_tx_clear();
    ws_station_poll();
    EXPECT(tx_has("t=2508"));
    EXPECT(tx_has("p="));
    EXPECT_EQ(ws_station_samples(), 1u);
    EXPECT((ws_station_faults() & (WS_FAULT_BME_TIMEOUT | WS_FAULT_BME_RANGE | WS_FAULT_BME_STALE)) ==
           0u);

    ws_lcd_copy(line0, line1);
    EXPECT(strstr(line0, "25.1C") != NULL);

    ws_host_rx("SET ALT 120\n");
    ws_station_poll();
    EXPECT(tx_has("OK\n"));
    ws_host_tx_clear();
    ws_host_rx("CFG\n");
    ws_station_poll();
    EXPECT(tx_has("alt=120"));
    EXPECT(tx_has("dirty=1") || tx_has("unit=M"));
    ws_host_tx_clear();
    ws_host_rx("STATUS\n");
    ws_station_poll();
    EXPECT(tx_has("dirty=1"));

    writes = ws_host_eeprom_writes();
    ws_host_rx("SAVE\n");
    ws_station_poll();
    EXPECT(tx_has("OK saved"));
    EXPECT(ws_host_eeprom_writes() > writes);
    writes = ws_host_eeprom_writes();
    ws_host_rx("SAVE\n");
    ws_station_poll();
    EXPECT_EQ(ws_host_eeprom_writes(), writes);

    ws_host_reboot();
    fake_bme_attach(&fake);
    ws_hal_init();
    ws_station_init();
    ws_host_tx_clear();
    ws_host_rx("CFG\n");
    ws_station_poll();
    EXPECT(tx_has("alt=120"));
    ws_host_tx_clear();
    ws_host_rx("STATUS\n");
    ws_station_poll();
    EXPECT(tx_has("dirty=0"));

    ws_eeprom_write(4, (uint8_t)(ws_eeprom_read(4) ^ 0xFFu));
    ws_host_reboot();
    fake_bme_attach(&fake);
    ws_hal_init();
    ws_station_init();
    EXPECT((ws_station_faults() & WS_FAULT_CONFIG) != 0u);
    ws_host_tx_clear();
    ws_host_rx("CFG\n");
    ws_station_poll();
    EXPECT(tx_has("alt=0"));

    boot(&fake, true);
    ws_host_tx_clear();
    ws_host_rx("SET PERIOD 100\n");
    ws_station_poll();
    EXPECT(tx_has("ERR range"));
    ws_host_tx_clear();
    ws_host_rx("SET PERIOD 500\n");
    ws_station_poll();
    EXPECT(tx_has("OK\n"));
    ws_host_rx("SET TELEM OFF\n");
    ws_station_poll();
    ws_host_tx_clear();
    ws_host_advance_ms(500);
    ws_station_poll();
    EXPECT(!tx_has("WS t="));
    ws_host_rx("READ\n");
    ws_station_poll();
    EXPECT(tx_has("WS t="));

    boot(&fake, true);
    ws_station_poll();
    ws_host_set_adc(700);
    ws_host_advance_ms(10);
    ws_station_poll();
    ws_host_advance_ms(10);
    ws_station_poll();
    ws_host_advance_ms(10);
    ws_station_poll();
    ws_host_set_adc(1023);
    ws_host_advance_ms(10);
    ws_station_poll();
    ws_host_advance_ms(10);
    ws_station_poll();
    ws_host_advance_ms(10);
    ws_station_poll();
    ws_lcd_copy(line0, line1);
    EXPECT(strstr(line0, "25.1C") != NULL);
    ws_host_advance_ms(4000);
    ws_station_poll();
    ws_lcd_copy(line0, line1);
    EXPECT(strstr(line0, "25.1C") != NULL);
    EXPECT(strstr(line0, "Dew") == NULL);

    ws_host_tx_clear();
    ws_host_rx("RESET\n");
    ws_station_poll();
    EXPECT(tx_has("OK reset"));
    EXPECT(ws_host_reset_requested());

    ws_host_reset();
    ws_host_set_reset_reason(WS_RESET_WATCHDOG);
    fake_bme_init(&fake);
    fake_bme_attach(&fake);
    ws_hal_init();
    ws_station_init();
    EXPECT((ws_station_faults() & WS_FAULT_WDT_RESET) != 0u);
    EXPECT(tx_has("reset=WDT"));
}
