#include "harness.h"

#include "ws/config_store.h"
#include "ws/crc16.h"
#include "ws/format.h"
#include "ws/hal_host.h"
#include "ws/keypad.h"
#include "ws/lcd.h"
#include "ws/metrics.h"
#include "ws/protocol.h"

#include <string.h>

void test_crc(void);
void test_format(void);
void test_keypad(void);
void test_metrics(void);
void test_config(void);
void test_protocol(void);
void test_lcd(void);

void test_crc(void)
{
    const uint8_t digits[] = "123456789";
    EXPECT_EQ(ws_crc16(digits, 9), 0x29B1);
    EXPECT_EQ(ws_crc16(digits, 0), 0xFFFF);
    EXPECT(ws_crc16(digits, 9) != ws_crc16(digits, 8));
}

void test_format(void)
{
    char buf[16];

    EXPECT_STR((ws_fmt_i32(buf, sizeof buf, 0), buf), "0");
    EXPECT_STR((ws_fmt_i32(buf, sizeof buf, -12), buf), "-12");
    EXPECT_STR((ws_fmt_i32(buf, sizeof buf, 2508), buf), "2508");
    EXPECT_EQ((int32_t)ws_fmt_i32(buf, 3, 2508), 4);
    EXPECT_STR(buf, "25");
    EXPECT_STR((ws_fmt_fixed(buf, sizeof buf, 2508, 2, 1), buf), "25.1");
    EXPECT_STR((ws_fmt_fixed(buf, sizeof buf, -405, 2, 1), buf), "-4.1");
    EXPECT_STR((ws_fmt_fixed(buf, sizeof buf, -4, 2, 1), buf), "0.0");
    EXPECT_STR((ws_fmt_fixed(buf, sizeof buf, 10065, 1, 1), buf), "1006.5");
    EXPECT_EQ(ws_fmt_hex16(buf, sizeof buf, 0, 0x001A), 4);
    EXPECT_STR(buf, "001A");
}

void test_keypad(void)
{
    EXPECT_EQ(ws_keypad_decode(0), WS_KEY_RIGHT);
    EXPECT_EQ(ws_keypad_decode(49), WS_KEY_RIGHT);
    EXPECT_EQ(ws_keypad_decode(50), WS_KEY_UP);
    EXPECT_EQ(ws_keypad_decode(249), WS_KEY_UP);
    EXPECT_EQ(ws_keypad_decode(250), WS_KEY_DOWN);
    EXPECT_EQ(ws_keypad_decode(449), WS_KEY_DOWN);
    EXPECT_EQ(ws_keypad_decode(450), WS_KEY_LEFT);
    EXPECT_EQ(ws_keypad_decode(649), WS_KEY_LEFT);
    EXPECT_EQ(ws_keypad_decode(650), WS_KEY_SELECT);
    EXPECT_EQ(ws_keypad_decode(849), WS_KEY_SELECT);
    EXPECT_EQ(ws_keypad_decode(850), WS_KEY_NONE);
    EXPECT_EQ(ws_keypad_decode(1023), WS_KEY_NONE);
}

void test_metrics(void)
{
    bool applicable = true;

    /* Alduchov & Eskridge Magnus constants, over water, rounded to 0.01 C. */
    EXPECT_NEAR(ws_dew_point_c_x100(2000, 5000), 926, 15);
    EXPECT_NEAR(ws_dew_point_c_x100(2500, 6000), 1670, 15);
    EXPECT_NEAR(ws_dew_point_c_x100(0, 10000), 0, 2);
    EXPECT_NEAR(ws_dew_point_c_x100(2500, 10000), 2500, 2);
    EXPECT_NEAR(ws_dew_point_c_x100(3000, 2000), 460, 15);
    EXPECT_NEAR(ws_dew_point_c_x100(-1000, 8000), -1280, 15);
    EXPECT_NEAR(ws_dew_point_c_x100(-500, 4000), -1653, 15);
    EXPECT_NEAR(ws_dew_point_c_x100(3500, 9000), 3311, 15);
    EXPECT_EQ(ws_dew_point_c_x100(2000, 0), -8000);

    EXPECT_NEAR(ws_heat_index_f_x100(7000, 4000, &applicable), 6858, 40);
    EXPECT(!applicable);
    EXPECT_NEAR(ws_heat_index_f_x100(8000, 4000, &applicable), 7958, 40);
    EXPECT(!applicable);
    EXPECT_NEAR(ws_heat_index_f_x100(9000, 5000, &applicable), 9460, 40);
    EXPECT(applicable);
    EXPECT_NEAR(ws_heat_index_f_x100(9500, 6000, &applicable), 11309, 40);
    EXPECT(applicable);
    EXPECT_NEAR(ws_heat_index_f_x100(10000, 4000, &applicable), 10926, 40);
    EXPECT(applicable);
    EXPECT_NEAR(ws_heat_index_f_x100(8500, 9000, &applicable), 10178, 40);
    EXPECT(applicable);
    EXPECT_NEAR(ws_heat_index_f_x100(9000, 1000, &applicable), 8528, 50);
    EXPECT(applicable);
    EXPECT_NEAR(ws_heat_index_f_x100(7500, 8000, &applicable), 7596, 40);
    EXPECT(!applicable);

    EXPECT_EQ(ws_sea_level_pa(101325u, 1500, 0), 101325u);
    EXPECT_NEAR((int32_t)ws_sea_level_pa(101325u, 1500, 100), 102533, 40);
    EXPECT_NEAR((int32_t)ws_sea_level_pa(100000u, 2000, 500), 106000, 80);
    EXPECT_NEAR((int32_t)ws_sea_level_pa(101325u, 1000, -100), 100110, 40);

    {
        ws_tendency_t tend;
        int i;
        ws_tendency_init(&tend);
        for (i = 0; i < 15; i++) {
            EXPECT_EQ(ws_tendency_update(&tend, 100000u, (uint32_t)i * 60000u), WS_TEND_UNKNOWN);
        }
        EXPECT_EQ(ws_tendency_update(&tend, 100000u, 15u * 60000u), WS_TEND_STEADY);
        EXPECT_EQ(ws_tendency_update(&tend, 100000u, 15u * 60000u + 1000u), WS_TEND_STEADY);
        for (i = 0; i < 16; i++) {
            uint32_t pa = 100000u + (uint32_t)(i + 1) * 100u;
            (void)ws_tendency_update(&tend, pa, (uint32_t)(16 + i) * 60000u);
        }
        EXPECT_EQ(tend.current, WS_TEND_RISING);
    }
}

void test_config(void)
{
    ws_config_t cfg;
    ws_config_t loaded;
    uint32_t writes;

    ws_host_reset();
    EXPECT_EQ(ws_config_load(&loaded), WS_CFG_BLANK);
    EXPECT_EQ(loaded.altitude_m, 0);
    EXPECT_EQ(loaded.period_ms, 1000);
    EXPECT_EQ(loaded.units, WS_UNIT_METRIC);

    ws_config_defaults(&cfg);
    cfg.altitude_m = -120;
    cfg.units = WS_UNIT_IMPERIAL;
    cfg.period_ms = 2500;
    cfg.telemetry = 0;
    EXPECT(ws_config_save(&cfg));
    writes = ws_host_eeprom_writes();
    EXPECT(writes > 0u);
    EXPECT(ws_config_save(&cfg));
    EXPECT_EQ(ws_host_eeprom_writes(), writes);

    EXPECT_EQ(ws_config_load(&loaded), WS_CFG_OK);
    EXPECT_EQ(loaded.altitude_m, -120);
    EXPECT_EQ(loaded.units, WS_UNIT_IMPERIAL);
    EXPECT_EQ(loaded.period_ms, 2500);
    EXPECT_EQ(loaded.telemetry, 0);

    ws_eeprom_write(4, (uint8_t)(ws_eeprom_read(4) ^ 0x01u));
    EXPECT_EQ(ws_config_load(&loaded), WS_CFG_CORRUPT);
    EXPECT_EQ(loaded.altitude_m, 0);

    cfg.period_ms = 100;
    EXPECT(!ws_config_save(&cfg));
    cfg.altitude_m = 9000;
    cfg.period_ms = 1000;
    EXPECT(!ws_config_in_range(&cfg));
}

void test_protocol(void)
{
    char line[96];
    ws_telem_t telem;
    ws_command_t cmd;

    cmd = ws_parse_command("set alt 120");
    EXPECT_EQ(cmd.id, WS_CMD_SET_ALT);
    EXPECT_EQ(cmd.arg, 120);
    cmd = ws_parse_command("SET ALT -400");
    EXPECT_EQ(cmd.id, WS_CMD_SET_ALT);
    EXPECT_EQ(cmd.arg, -400);
    cmd = ws_parse_command("SET UNIT i");
    EXPECT_EQ(cmd.id, WS_CMD_SET_UNIT);
    EXPECT_EQ(cmd.arg, 1);
    cmd = ws_parse_command("SET UNIT M extra");
    EXPECT_EQ(cmd.id, WS_CMD_SYNTAX);
    cmd = ws_parse_command("SET PERIOD 500");
    EXPECT_EQ(cmd.id, WS_CMD_SET_PERIOD);
    EXPECT_EQ(cmd.arg, 500);
    cmd = ws_parse_command("SET PERIOD");
    EXPECT_EQ(cmd.id, WS_CMD_SYNTAX);
    cmd = ws_parse_command("SET TELEM off");
    EXPECT_EQ(cmd.id, WS_CMD_SET_TELEM);
    EXPECT_EQ(cmd.arg, 0);
    cmd = ws_parse_command("NOPE");
    EXPECT_EQ(cmd.id, WS_CMD_UNKNOWN);
    cmd = ws_parse_command("");
    EXPECT_EQ(cmd.id, WS_CMD_NONE);
    cmd = ws_parse_command("VER now");
    EXPECT_EQ(cmd.id, WS_CMD_SYNTAX);
    cmd = ws_parse_command("?");
    EXPECT_EQ(cmd.id, WS_CMD_HELP);

    memset(&telem, 0, sizeof telem);
    telem.temp_c_x100 = 2508;
    telem.rh_x100 = 5000;
    telem.pressure_pa = 101325u;
    telem.sea_level_pa = 101325u;
    telem.dew_c_x100 = 1384;
    telem.heat_applicable = false;
    telem.tendency = WS_TEND_UNKNOWN;
    telem.faults = 0x0010u;
    telem.units = 0u;
    EXPECT(ws_format_telemetry(line, sizeof line, &telem) < sizeof line);
    EXPECT_STR(line, "WS t=2508 h=5000 p=101325 sl=101325 td=1384 hi=na tr=9 f=0010 u=0\n");
    telem.heat_applicable = true;
    telem.heat_c_x100 = 3478;
    telem.tendency = WS_TEND_RISING;
    (void)ws_format_telemetry(line, sizeof line, &telem);
    EXPECT(strstr(line, "hi=3478") != NULL);
    EXPECT(strstr(line, "tr=1") != NULL);
}

void test_lcd(void)
{
    char line0[17];
    char line1[17];
    bool rs = true;
    uint8_t nibble = 0xFF;
    static const uint8_t expect[] = {0x3, 0x3, 0x3, 0x2, 0x2, 0x8, 0x0, 0x8,
                                     0x0, 0x1, 0x0, 0x6, 0x0, 0xC};
    size_t i;
    size_t after_init;

    ws_host_reset();
    ws_host_panel_clear();
    ws_lcd_init();
    EXPECT(ws_host_panel_count() >= sizeof expect);
    for (i = 0; i < sizeof expect; i++) {
        ws_host_panel_at(i, &rs, &nibble);
        EXPECT(!rs);
        EXPECT_EQ(nibble, expect[i]);
    }
    ws_lcd_draw("Hello", "world");
    ws_lcd_copy(line0, line1);
    EXPECT_STR(line0, "Hello           ");
    EXPECT_STR(line1, "world           ");
    after_init = ws_host_panel_count();
    ws_lcd_draw("Hello", "world");
    EXPECT_EQ((int32_t)ws_host_panel_count(), (int32_t)after_init);
}
