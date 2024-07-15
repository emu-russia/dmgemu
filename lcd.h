#pragma once

#define SCALE 2

extern int lcd_fpslimit;

extern uint8_t linebuffer[192];

extern HWND main_hwnd;

extern unsigned benchmark_sound, benchmark_gfx;

void win32_win_init(int width, int height);
void win32_win_update();
void WIN_Center(HWND hwnd);
void win32_dib_init(int width, int height);
void win32_dib_shutdown();
void win32_dib_blit();
void lcd_refresh(int line);
