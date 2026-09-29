#include "ws/keypad.h"

ws_key_t ws_keypad_decode(uint16_t adc)
{
    if (adc < 50u) {
        return WS_KEY_RIGHT;
    }
    if (adc < 250u) {
        return WS_KEY_UP;
    }
    if (adc < 450u) {
        return WS_KEY_DOWN;
    }
    if (adc < 650u) {
        return WS_KEY_LEFT;
    }
    if (adc < 850u) {
        return WS_KEY_SELECT;
    }
    return WS_KEY_NONE;
}
