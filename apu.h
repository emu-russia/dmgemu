#pragma once

#define SO_FREQ (1<<20)

#define RI_NR10 0x10
#define RI_NR11 0x11
#define RI_NR12 0x12
#define RI_NR13 0x13
#define RI_NR14 0x14
#define RI_NR21 0x16
#define RI_NR22 0x17
#define RI_NR23 0x18
#define RI_NR24 0x19
#define RI_NR30 0x1A
#define RI_NR31 0x1B
#define RI_NR32 0x1C
#define RI_NR33 0x1D
#define RI_NR34 0x1E
#define RI_NR41 0x20
#define RI_NR42 0x21
#define RI_NR43 0x22
#define RI_NR44 0x23
#define RI_NR50 0x24
#define RI_NR51 0x25
#define RI_NR52 0x26

//extern uint32_t apu_clk;  - same as gb_clk now
extern uint32_t apu_clk_inner[2];
extern uint32_t apu_clk_nextchange;
// ALL internal clock variables are exported (to be wrapped in gb.c)


uint8_t apu_read(uint8_t);
void apu_write(uint8_t, uint8_t);

void apu_init(int freq);
void apu_shutdown();
void apu_mix();
