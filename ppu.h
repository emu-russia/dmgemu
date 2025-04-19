#pragma once

void ppu_init();
void ppu_shutdown();
void ppu_enumsprites();
void ppu_refreshline();
void ppu_vsync();


extern unsigned lcd_WYline;
extern uint8_t linebuffer[192];

#define CONVPAL(to,data) \
	to[0] = (data >> (0 << 1)) & 3;\
	to[1] = (data >> (1 << 1)) & 3;\
	to[2] = (data >> (2 << 1)) & 3;\
	to[3] = (data >> (3 << 1)) & 3;

extern unsigned mainpal[64];
#define BGP mainpal
#define OBP0 (mainpal+4)
#define OBP1 (mainpal+8)

extern unsigned benchmark_sound,benchmark_gfx;
extern uint8_t tilecache[512];
