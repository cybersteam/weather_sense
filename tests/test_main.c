#include "harness.h"

#include <stdio.h>
#include <unistd.h>

void test_crc(void);
void test_format(void);
void test_keypad(void);
void test_metrics(void);
void test_config(void);
void test_protocol(void);
void test_lcd(void);
void test_bme(void);
void test_ui(void);
void test_station(void);

int main(void)
{
    /* A busy-wait regression should die here instead of hanging the suite. */
    alarm(10);

    test_crc();
    test_format();
    test_keypad();
    test_metrics();
    test_config();
    test_protocol();
    test_lcd();
    test_bme();
    test_ui();
    test_station();

    printf("passed=%d failed=%d\n", ws_test_passes(), ws_test_failures());
    return ws_test_failures() == 0 ? 0 : 1;
}
