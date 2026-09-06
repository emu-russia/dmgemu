/* DMG memory-mapped IO: divider and timer (FF04-FF07) */
#include "pch.h"

/* ======================= DMG timer (DIV/TIMA/TMA/TAC) =======================
   Model (pandocs/mooneye): the divider is a chain that ticks every T-cycle;
   DIV (FF04) is its upper byte (16384 Hz, +1 per 64 M-cycles).  TIMA is
   incremented on the falling edge of a chain bit selected by TAC:

     select 00 -> 4096 Hz   = one increment per 256 M-cycles  (tap bit 9)
     select 01 -> 262144 Hz = one increment per   4 M-cycles  (tap bit 3)
     select 10 -> 65536 Hz  = one increment per  16 M-cycles  (tap bit 5)
     select 11 -> 16384 Hz  = one increment per  64 M-cycles  (tap bit 7)

   Increment times are aligned to the divider chain (every P M-cycles counted
   from the last divider reset), and toggling TAC does *not* reset that phase.

   On overflow (0xFF->0x00) the TIMA register holds $00 for one M-cycle (the
   real hardware keeps it zero for 4 T-cycles) and is then reloaded from TMA;
   the timer interrupt (bit 2 of IF) is requested at the overflow instant.

   gb_clk is in M-cycles (1.048576 MHz = 4.194304MHz / 4).             */

static const uint8_t timshift_tab[4] = { 8, 2, 4, 6 };   // period = 1<<shift M-cycles

static uint32_t div_epoch = 0;  // gb_clk of the last divider (chain) reset
static uint8_t  tim_en = 0;     // TIMA clock enabled (TAC bit 2)
static uint8_t  tim_shift = 8;  // current tap period shift (M-cycles per increment)

static uint32_t tcur = 0;       // time up to which the state below is valid
static int      vcur = 0;       // TIMA value at tcur (0..255)
static uint32_t reload_at = MAXULONG;   // time TIMA is reloaded from TMA (MAXULONG = none)
static uint8_t  reload_val = 0;         // value loaded at reload_at

/* number of tap falls in (div_epoch, t]  (i.e. at div_epoch + P*k) */
static uint32_t falls_le(uint32_t t)
{
	if (t <= div_epoch) return 0;
	return (t - div_epoch) >> tim_shift;
}

/* advance the model from tcur to tnow, applying every event in between:
   - tap falls increment TIMA (an overflow makes it 0 and arms a TMA reload
     one M-cycle later, requesting the timer interrupt at the same instant) */
static void step_to(uint32_t tnow)
{
	if (!tim_en || tnow <= tcur) return;
	for (;;) {
		uint32_t nf = div_epoch + ((falls_le(tcur) + 1u) << tim_shift); // next fall > tcur
		uint32_t ev = reload_at;
		if (nf < ev) ev = nf;
		if (ev > tnow) break;
		if (ev == reload_at) {
			/* reload TIMA from TMA (the $00 hold lasted one M-cycle) */
			vcur = reload_val;
			tcur = reload_at;
			reload_at = MAXULONG;
		} else {
			/* tap fall: TIMA increments */
			tcur = nf;
			vcur++;
			if (vcur > 255) {
				/* overflow: register holds 0 for one M-cycle, then TMA */
				vcur = 0;
				reload_at = tcur + 1;
				reload_val = R_TMA;
				R_IF |= INT_TIMER;	// request timer interrupt
			}
		}
	}
	R_TIMA = (uint8_t)vcur;
}

/* ------------------------------------------------------------------ */
uint8_t mmio_div_read(void)				// FF04 - divider
{
	uint32_t e = (gb_clk > div_epoch) ? (gb_clk - div_epoch) : 0;
	return (uint8_t)(e >> 6);
}

uint8_t mmio_tima_read(void)				// FF05 - timer counter
{
	step_to(gb_clk);
	return R_TIMA;
}

void mmio_tima_write(unsigned data)			// write to FF05
{
	step_to(gb_clk);
	if (tim_en && reload_at != MAXULONG && reload_at > gb_clk) {
		/* A write landing inside the 1 M-cycle "$00 hold" window is
		   discarded: the pending TMA reload overwrites it anyway. */
		return;
	}
	R_TIMA = (uint8_t)data;
	vcur = (int)(uint8_t)data;
	tcur = gb_clk;
}

/* write to FF07: enable/disable the timer and change its clock select.
   Toggling TAC never resets the divider phase. */
void mmio_tac_write(unsigned data)
{
	unsigned ntac = data & 7;
	if ((R_TAC & 7) == ntac) return;
	if (tim_en) {
		step_to(gb_clk);			// keep current value/state
		if (!(ntac & 4)) {			// disabling: freeze TIMA
			R_TIMA = (uint8_t)vcur;
			tim_en = 0;
			reload_at = MAXULONG;		// pending reload no longer applies
			R_TAC = (uint8_t)ntac;
			return;
		}
	}
	R_TAC = (uint8_t)ntac;
	if (ntac & 4) {				// starting / clock change
		tim_shift = timshift_tab[ntac & 3];
		tim_en = 1;
		tcur = gb_clk;			// resume counting from now, chain-aligned
		vcur = (int)R_TIMA;
	} else {
		tim_en = 0;
	}
}

/* write to FF04 (or STOP): the divider chain is reset; since the timer taps
   come from the same chain this also re-aligns the timer phase (mooneye
   div_write: frequent DIV resets must prevent any timer increment).        */
void mmio_div_write(void)
{
	step_to(gb_clk);
	div_epoch = gb_clk;
	tcur = gb_clk;
	vcur = (int)R_TIMA;			// TIMA itself is not cleared
}

/* ------------------------------------------------------------------ */
/* scheduler interface (used from gb.cpp) */

uint32_t mmio_timer_next_event(void)
{
	if (!tim_en) return MAXULONG;
	/* simulate events from the current state until the next overflow */
	uint32_t t = tcur;
	int v = vcur;
	uint32_t ra = reload_at;
	uint8_t rv = reload_val;
	for (;;) {
		uint32_t nf = div_epoch + ((falls_le(t) + 1u) << tim_shift);
		uint32_t ev = ra;
		if (nf < ev) ev = nf;
		if (ev == ra) {		// reload
			v = rv;
			t = ra;
			ra = MAXULONG;
			if (t == MAXULONG) return MAXULONG;	// no more events?!
		} else {		// fall
			t = nf;
			v++;
			if (v > 255) return t;
		}
	}
}

void mmio_timer_fire(void)
{
	step_to(gb_clk);
}

void mmio_timer_rewind(uint32_t delta)
{
	if (div_epoch > delta) div_epoch -= delta; else div_epoch = 0;
	if (tcur > delta) tcur -= delta; else tcur = 0;
	if (reload_at != MAXULONG) {
		if (reload_at > delta) reload_at -= delta; else reload_at = 0;
	}
}

void mmio_timer_init(void)
{
	div_epoch = 0;
	tcur = 0;
	vcur = 0;
	reload_at = MAXULONG;
	reload_val = 0;
	tim_en = 0;
	tim_shift = timshift_tab[0];
}
