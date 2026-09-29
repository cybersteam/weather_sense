#include "harness.h"

#include "ws/faults.h"
#include "ws/hal.h"
#include "ws/metrics.h"
#include "ws/ui.h"

#include <string.h>

void test_ui(void);

static ws_ui_input_t sample_ui(void)
{
    ws_ui_input_t in;
    memset(&in, 0, sizeof in);
    in.have_sample = true;
    in.temp_c_x100 = 2508;
    in.rh_x100 = 4439;
    in.pressure_pa = 100653u;
    in.sea_level_pa = 102046u;
    in.dew_c_x100 = 1234;
    in.heat_applicable = false;
    in.tendency = WS_TEND_RISING;
    in.page = 0;
    in.altitude_m = 120;
    in.units = 0;
    in.uptime_s = 12;
    in.reset_reason = WS_RESET_POWER_ON;
    return in;
}

void test_ui(void)
{
    ws_ui_input_t in = sample_ui();
    char line0[17];
    char line1[17];

    ws_ui_format(&in, line0, line1);
    EXPECT_EQ((int32_t)strlen(line0), 16);
    EXPECT_EQ((int32_t)strlen(line1), 16);
    EXPECT_STR(line0, "25.1C        44%");
    EXPECT_STR(line1, "1006.5hPa      ^");

    in.units = 1;
    ws_ui_format(&in, line0, line1);
    EXPECT_STR(line0, "77.1F        44%");
    EXPECT(strstr(line1, "inHg") != NULL);

    in = sample_ui();
    in.page = 1;
    ws_ui_format(&in, line0, line1);
    EXPECT(strncmp(line0, "Dew", 3) == 0);
    EXPECT(strstr(line0, "12.3C") != NULL);
    EXPECT(strncmp(line1, "MSL", 3) == 0);

    in.page = 2;
    ws_ui_format(&in, line0, line1);
    EXPECT_STR(line0, "Heat         n/a");
    EXPECT_STR(line1, "Up           12s");

    in.uptime_s = 3723;
    ws_ui_format(&in, line0, line1);
    EXPECT(strstr(line1, "1h02m") != NULL);

    in.page = 3;
    in.faults = 0;
    ws_ui_format(&in, line0, line1);
    EXPECT_STR(line0, "System OK       ");
    EXPECT_STR(line1, "Reset        POR");

    in.hold = true;
    ws_ui_format(&in, line0, line1);
    EXPECT(strstr(line0, "HOLD") != NULL);

    in.faults = WS_FAULT_BME_STALE;
    in.hold = false;
    ws_ui_format(&in, line0, line1);
    EXPECT_STR(line0, "FAULT       0008");
    EXPECT_STR(line1, "BME stale       ");

    in = sample_ui();
    in.have_sample = false;
    in.faults = WS_FAULT_BME_ID;
    in.page = 0;
    ws_ui_format(&in, line0, line1);
    EXPECT(strstr(line1, "BME missing") != NULL);

    in.editing = true;
    in.altitude_m = 120;
    in.units = 0;
    ws_ui_format(&in, line0, line1);
    EXPECT_STR(line0, "Alt         120m");
    EXPECT_STR(line1, "UP/DN SEL=save  ");
}
