#pragma once

void ws_lcd_init(void);
/* Accepts a short C string or a full 16-character row. Missing cells are spaces. */
void ws_lcd_draw(const char *line0, const char *line1);
void ws_lcd_copy(char line0[17], char line1[17]);
