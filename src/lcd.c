#include "ws/lcd.h"

#include "ws/hal.h"

static char shadow[2][16];

static void write_nibble_cmd(uint8_t nibble)
{
    ws_panel_write(false, nibble);
}

static void write_byte(bool rs, uint8_t value)
{
    ws_panel_write(rs, (uint8_t)(value >> 4));
    ws_panel_write(rs, (uint8_t)(value & 0x0Fu));
    /* Clear and home need 1.52 ms. Every other instruction needs 37 us. */
    if (!rs && (value == 0x01u || value == 0x02u)) {
        ws_delay_ms(2);
    } else {
        ws_delay_us(50);
    }
}

void ws_lcd_init(void)
{
    unsigned row;
    unsigned col;

    /* HD44780 power-on sequence into 4-bit, 2-line mode. R/W is hard-wired low. */
    ws_delay_ms(40);
    write_nibble_cmd(0x03u);
    ws_delay_ms(5);
    write_nibble_cmd(0x03u);
    ws_delay_us(150);
    write_nibble_cmd(0x03u);
    ws_delay_us(150);
    write_nibble_cmd(0x02u);
    ws_delay_us(150);
    write_byte(false, 0x28u);
    write_byte(false, 0x08u);
    write_byte(false, 0x01u);
    write_byte(false, 0x06u);
    write_byte(false, 0x0Cu);
    for (row = 0; row < 2u; row++) {
        for (col = 0; col < 16u; col++) {
            shadow[row][col] = (char)0xFF;
        }
    }
}

static char cell_at(const char *text, int index)
{
    int i;
    char c;

    if (text == NULL) {
        return ' ';
    }
    for (i = 0; i <= index; i++) {
        if (text[i] == '\0') {
            return ' ';
        }
    }
    c = text[index];
    if (c < 32 || c > 126) {
        return ' ';
    }
    return c;
}

static void draw_row(int row, uint8_t base, const char *text)
{
    int col;

    for (col = 0; col < 16; col++) {
        char c = cell_at(text, col);
        if (shadow[row][col] == c) {
            continue;
        }
        write_byte(false, (uint8_t)(0x80u | (uint8_t)(base + col)));
        write_byte(true, (uint8_t)c);
        shadow[row][col] = c;
    }
}

void ws_lcd_draw(const char *line0, const char *line1)
{
    draw_row(0, 0x00u, line0);
    draw_row(1, 0x40u, line1);
}

void ws_lcd_copy(char line0[17], char line1[17])
{
    int col;

    for (col = 0; col < 16; col++) {
        char c0 = shadow[0][col];
        char c1 = shadow[1][col];
        line0[col] = (c0 == (char)0xFF) ? ' ' : c0;
        line1[col] = (c1 == (char)0xFF) ? ' ' : c1;
    }
    line0[16] = '\0';
    line1[16] = '\0';
}
