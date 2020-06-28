//extern unsigned long so_clk;  - same as gb_clk now
extern unsigned long so_clk_inner[2];
extern unsigned long so_clk_nextchange;
// ALL internal clock variables are exported (to be wrapped in gb.c)


uint8_t so_read(uint8_t);
void so_write(uint8_t, uint8_t);

void so_init(unsigned long);
void so_shutdown();
void so_mix();
