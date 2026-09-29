#pragma once

#include <stdint.h>

/* Bands of the DFRobot LCD keypad shield resistor ladder on A0. */
typedef enum {
    WS_KEY_NONE = 0,
    WS_KEY_RIGHT,
    WS_KEY_UP,
    WS_KEY_DOWN,
    WS_KEY_LEFT,
    WS_KEY_SELECT
} ws_key_t;

ws_key_t ws_keypad_decode(uint16_t adc);
