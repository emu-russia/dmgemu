#pragma once

/* Interrupt flags*/
#define INT_NONE	0
#define INT_VBLANK	1
#define INT_LCDSTAT 2
#define INT_TIMER   4
#define INT_SERIAL  8
#define INT_PAD		0x10
#define INT_ALL		0x1F


void gb_init(void);
void gb_shutdown(void);
void start(void);
void check4LYC(void);

//void check4LCDint(void);


extern uint32_t gb_clk;

#define BENCHMARK 0

extern unsigned benchmark_sound, benchmark_gfx;
extern int sound_enabled;