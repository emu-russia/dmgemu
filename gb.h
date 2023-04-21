#pragma once

void gb_init(void);
void gb_shutdown(void);
void start(void);
void check4LYC(void);
void gb_reload_tima(unsigned data);

//void check4LCDint(void);


extern unsigned long gb_clk;
extern unsigned long gb_timerclk; // time before next timer interrupt
extern unsigned long gb_divbase;
extern unsigned long gb_timbase;
extern uint8_t gb_timshift;
