//extern unsigned long so_clk;  - same as gb_clk now
extern unsigned long so_clk_inner[2];
extern unsigned long so_clk_nextchange;
// ALL internal clock variables are exported (to be wrapped in gb.c)


byte so_read(byte);
void so_write(byte, byte);

void so_init(unsigned long);
void so_shutdown();
void so_mix();