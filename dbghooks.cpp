/* Debug hooks - only compiled when DMGEMU_DEBUG_HOOKS is defined
   (see dbghooks.h).  Kept out of release builds.                       */
#include "pch.h"

#ifdef DMGEMU_DEBUG_HOOKS

/* =================== CPU instruction trace =================== */
/* Enabled by env vars:
     DMGEMU_TRACE     = path of trace file (e.g. C:\temp\dmgemu_trace.log)
     DMGEMU_TRACE_MAX = optional cap on number of instructions logged   */
static FILE *trace_file = NULL;
static uint64_t trace_n = 0;
static uint64_t trace_max = 0;
static int trace_tried = 0;

void dbg_cpu_trace(uint32_t pc, unsigned opcode, uint32_t clk)
{
	if (!trace_tried) {
		trace_tried = 1;
		const char *name = getenv("DMGEMU_TRACE");
		if (name && name[0]) {
			trace_file = fopen(name, "wt");
			const char *mx = getenv("DMGEMU_TRACE_MAX");
			if (mx) trace_max = (uint64_t)atoll(mx);
			else trace_max = ~0ULL;
			fprintf(trace_file, "; dmgemu CPU trace - PC OPCODE AF BC DE HL SP IME LY STAT IF IE CLK\n");
		}
	}
	if (trace_file == NULL) return;
	if (trace_n >= trace_max) return;
	fprintf(trace_file, "%04X %02X %04X %04X %04X %04X %04X %u %02X %02X %02X %02X %02X %08X b%d\n",
		pc, opcode, R_AF, R_BC, R_DE, R_HL, R_SP, IME != 0, R_LY, R_STAT, R_IF, R_IE, R_LCDC, clk,
		cart.rom[1].bank);
	trace_n++;
	if ((trace_n & 0x3FF) == 0) fflush(trace_file);
}

/* =================== per-frame state snapshot =================== */
/* DMGEMU_SNAP env: path of snapshot file. Every rendered frame appends:
   4-byte frame no, 16 bytes CPU regs (AF BC DE HL SP PC IME HALT),
   512 bytes hram (incl OAM at 0x00-0x9F and IO regs at 0x100+),
   0x2000 bytes VRAM, w*h*4 bytes pbuf (final displayed pixels).       */
void dbg_snap_frame(uint32_t const* pbuf, int w, int h)
{
	static FILE *snap_file = NULL;
	static unsigned snap_frame = 0;
	static int snap_opened = 0;
	if (!snap_opened) {
		snap_opened = 1;
		const char *name = getenv("DMGEMU_SNAP");
		if (name && name[0]) snap_file = fopen(name, "wb");
	}
	if (!snap_file) return;
	struct { unsigned f; uint16_t af, bc, de, hl, sp, pc; unsigned ime, halt; } hdr;
	hdr.f = snap_frame++;
	hdr.af = R_AF; hdr.bc = R_BC; hdr.de = R_DE; hdr.hl = R_HL; hdr.sp = R_SP; hdr.pc = R_PC;
	hdr.ime = IME; hdr.halt = HALT;
	fwrite(&hdr, 1, sizeof(hdr), snap_file);
	fwrite(hram, 1, 0x200, snap_file);
	fwrite(vram, 1, 0x2000, snap_file);
	fwrite(pbuf, 4, (size_t)w * h, snap_file);
}

/* =================== fatal event log =================== */
/* DMGEMU_FATAL_LOG env: append every show_regs / sys_error payload here
   (plus CPU state), so crashes like "Undefined opcode" can be inspected
   from the log file instead of only via message boxes.               */
void dbg_log_event(char const* tag, char const* text)
{
	static FILE *ev = NULL;
	static int ev_tried = 0;
	if (!ev_tried) {
		ev_tried = 1;
		const char *name = getenv("DMGEMU_FATAL_LOG");
		if (name && name[0]) ev = fopen(name, "a");
	}
	if (!ev) return;
	fprintf(ev, "======== %s ========\n", tag);
	fprintf(ev, "PC=%04X AF=%04X BC=%04X DE=%04X HL=%04X SP=%04X CLK=%u IME=%u HALT=%u\n",
		R_PC, R_AF, R_BC, R_DE, R_HL, R_SP, (unsigned)gb_clk, IME != 0, HALT != 0);
	fprintf(ev, "%s\n", text ? text : "(null)");
	fprintf(ev, "LCDC=%02X STAT=%02X LY=%02X IF=%02X IE=%02X TIMA=%02X TMA=%02X TAC=%02X DIV=%02X\n",
		R_LCDC, R_STAT, R_LY, R_IF, R_IE, R_TIMA, R_TMA, R_TAC, R_DIV);
	fflush(ev);
}

#endif /* DMGEMU_DEBUG_HOOKS */
