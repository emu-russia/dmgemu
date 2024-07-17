#pragma once

void ppu_init();
void ppu_shutdown();
void ppu_enumsprites();
void ppu_refreshline();
void ppu_vsync();


extern unsigned lcd_WYline;
extern uint8_t linebuffer[192];

extern unsigned mainpal[64];
#define BGP mainpal
#define OBP0 (mainpal+4)
#define OBP1 (mainpal+8)

extern unsigned benchmark_sound,benchmark_gfx;
extern uint8_t tilecache[512];
