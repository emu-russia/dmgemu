#pragma once

extern int lcd_scale;
extern int lcd_fpslimit;

void lcd_refresh(int line);

void sdl_win_init(int width, int height);
void sdl_win_shutdown();
void sdl_win_update();
void sdl_win_blit();
void sdl_win_update_title(char* title);

/* small message box over the bottom of the picture (save states, slots) */
void lcd_overlay_show(const char* text);

/* The picture itself is part of a save state: lcd_refresh() blends every new
   line with what is already in the buffer (the LCD "ghost" effect), so
   without it the screen right after a load would keep a trace of the frame
   that was displayed before the load. */
uint32_t* lcd_screen_buffer(void);  /* NULL when there is no window */
int       lcd_screen_pixels(void);  /* number of pixels in that buffer */
