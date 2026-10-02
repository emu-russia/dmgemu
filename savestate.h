#pragma once

/* Save states: a complete snapshot of the emulated machine in one of three
   slots (F5 - save, F6 - next slot, F7 - load, see lcd.cpp).
   Every module owns its own part of SaveState and fills it in its
   <module>_state_save() / <module>_state_load() pair, so adding state to a
   module does not touch the others.

   A state file is, in this order: a SaveState, cart.ram_size bytes of
   cartridge RAM and screen_size pixels of the picture (each part is simply
   missing when its size is zero, so the file can be written and read with a
   few fwrite/fread calls).                                             */

#include <stdint.h>

#define SAVESTATE_SLOTS     3
#define SAVESTATE_VERSION   1u
#define SAVESTATE_MAGIC     "DMGEMUSS"  /* exactly 8 bytes, no terminator */
#define SAVESTATE_MAGIC_LEN 8

/* savestate_load() result */
#define SS_LOAD_OK          0
#define SS_LOAD_EMPTY      -1   /* there is no state file in this slot */
#define SS_LOAD_BAD        -2   /* not a state file / another version or layout */
#define SS_LOAD_OTHER_ROM  -3   /* the state was made by another cartridge */

#pragma pack(push, 1)

/* ---- CPU (sm83.cpp) ---- */
struct SS_Cpu {
	uint16_t af, bc, de, hl, sp, pc;
	uint8_t  halt, ime, ime_delay;
};

/* ---- divider/timer (mmio.cpp) ---- */
struct SS_Timer {
	uint32_t div_epoch, tcur, reload_at;
	int32_t  vcur;
	uint8_t  tim_en, tim_shift, reload_val;
};

/* ---- joypad (pad.cpp) ---- */
struct SS_Pad {
	uint8_t hi, lo;
};

/* ---- mapper (cart.cpp) ---- */
struct SS_Mbc {
	int32_t  bank1, bank2, mode;
	uint32_t ramenabled, clock_present, clock_latched;
	uint32_t rom_bank0, rom_bank1, ram_bank;
};

/* ---- PPU (ppu.cpp) ---- */
struct SS_Ppu {
	uint8_t  linebuffer[192];
	uint32_t mainpal[64];
	uint8_t  used_spr[40 * 4];
	uint32_t num_sprites;
	int32_t  lcd_WYline;
};

/* ---- APU (apu.cpp) ---- */
struct SS_Apu {
	apu_state snd;
	uint32_t  clk_inner[2];
	uint32_t  clk_nextchange;
};

/* ---- memory (mem.cpp) ---- */
struct SS_Mem {
	uint8_t vram[0x2000];
	uint8_t ram[0x2000];
	uint8_t hram[0x200];
};

struct SaveState {
	/* ---- file header ---- */
	char     magic[SAVESTATE_MAGIC_LEN];
	uint32_t version;
	uint32_t state_size;        /* sizeof(SaveState) of the writer */
	uint32_t rom_size;          /* cartridge identity: size ... */
	uint32_t ram_size;          /* ... cart RAM bytes following this struct */
	uint32_t screen_size;       /* ... pixels of picture after those */
	uint8_t  rom_header[0x50];  /* ... and the ROM header ($0100-$014F) */

	/* ---- machine ---- */
	uint32_t gb_clk;
	uint32_t gb_eventclk;
	uint32_t lcd_int_on;

	SS_Cpu   cpu;
	SS_Timer timer;
	SS_Pad   pad;
	SS_Mbc   mbc;
	SS_Ppu   ppu;
	SS_Apu   apu;
	SS_Mem   mem;
};

#pragma pack(pop)

/* per-module state accessors */
void sm83_state_save(SaveState* st);
void sm83_state_load(const SaveState* st);
void mmio_state_save(SaveState* st);
void mmio_state_load(const SaveState* st);
void pad_state_save(SaveState* st);
void pad_state_load(const SaveState* st);
void cart_state_save(SaveState* st);
void cart_state_load(const SaveState* st);
void ppu_state_save(SaveState* st);
void ppu_state_load(const SaveState* st);
void apu_state_save(SaveState* st);
void apu_state_load(const SaveState* st);
void mem_state_save(SaveState* st);
void mem_state_load(const SaveState* st);
void gb_state_save(SaveState* st);
void gb_state_load(const SaveState* st);

/* slots (used by the F5/F6/F7 handlers) */
int  savestate_slot(void);          /* current slot, 0..SAVESTATE_SLOTS-1 */
void savestate_next_slot(void);     /* select the next slot */
int  savestate_save(int slot);      /* 1 = saved, 0 = write error */
int  savestate_load(int slot);      /* SS_LOAD_* */
