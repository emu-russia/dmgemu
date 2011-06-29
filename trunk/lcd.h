void lcd_init();
void lcd_shutdown();
void lcd_enumsprites();
void lcd_refreshline();
void lcd_vsync();



extern unsigned lcd_WYline;

extern unsigned mainpal[64];
#define BGP mainpal
#define OBP0 (mainpal+4)
#define OBP1 (mainpal+8)

PLAT extern unsigned benchmark_sound,benchmark_gfx;
extern byte tilecache[512];
