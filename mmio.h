#pragma once

/* Memory-mapped IO of the DMG SoC: divider & timer registers (FF04-FF07).
   The actual register bytes live in hram (R_DIV/R_TIMA/R_TMA/R_TAC in mem.h);
   this module implements their behavior (counting, overflow, interrupts).
   gb_clk is in M-cycles (1.048576 MHz = 4.194304MHz / 4).                */

uint8_t   mmio_div_read(void);              /* FF04 read: 16384Hz divider   */
void      mmio_div_write(void);             /* FF04 write (also on STOP): resets divider & timer phase */
uint8_t   mmio_tima_read(void);             /* FF05 read: current TIMA      */
void      mmio_tima_write(unsigned data);   /* FF05 write                   */
void      mmio_tac_write(unsigned data);    /* FF07 write: enable + clock select */

/* scheduler interface used by the main emulation loop (gb.cpp) */
uint32_t  mmio_timer_next_event(void);      /* gb_clk of next TIMA overflow (MAXULONG = off) */
void      mmio_timer_fire(void);            /* overflow happened now: reload TIMA from TMA,
                                               request the timer interrupt (IF bit 2) */
void      mmio_timer_rewind(uint32_t delta);/* subtract delta from pending event (clk wrap-around) */
void      mmio_timer_init(void);            /* reset all timer state at boot */
