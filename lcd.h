#pragma once

extern int lcd_scale;
extern int lcd_fpslimit;

void lcd_refresh(int line);

void sdl_win_init(int width, int height);
void sdl_win_shutdown();
void sdl_win_update();
void sdl_win_blit();
void sdl_win_update_title(char* title);
