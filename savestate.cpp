/* Save states (F5 - save, F6 - next slot, F7 - load) */
#include "pch.h"

static int savestate_current_slot = 0;

int savestate_slot(void)
{
	return savestate_current_slot;
}

void savestate_next_slot(void)
{
	savestate_current_slot = (savestate_current_slot + 1) % SAVESTATE_SLOTS;
}

static void savestate_file_name(int slot, char* name)
{
	/* same convention as the battery RAM: <cartridge title>.sav */
	sprintf(name, "%s.st%d", cart.title, slot);
}

/* ---------------- the state of every module ---------------- */

static void savestate_collect(SaveState* st)
{
	memset(st, 0, sizeof(SaveState));

	memcpy(st->magic, SAVESTATE_MAGIC, SAVESTATE_MAGIC_LEN);
	st->version = SAVESTATE_VERSION;
	st->state_size = (uint32_t)sizeof(SaveState);
	st->rom_size = cart.rom_size;
	st->ram_size = (cart.ram_size && cart.ramdata) ? cart.ram_size : 0;
	if (cart.data)
		memcpy(st->rom_header, cart.data + 0x100, sizeof(st->rom_header));

	gb_state_save(st);
	sm83_state_save(st);
	mem_state_save(st);
	cart_state_save(st);
	ppu_state_save(st);
	apu_state_save(st);
	mmio_state_save(st);
	pad_state_save(st);

	/* the picture follows the cartridge RAM in the file */
	st->screen_size = (uint32_t)lcd_screen_pixels();
}

static void savestate_apply(const SaveState* st)
{
	/* memory first (the PPU rebuilds its tile cache from VRAM), then the
	   mapper (it re-maps the RAM window), then everything else */
	mem_state_load(st);
	cart_state_load(st);
	ppu_state_load(st);
	apu_state_load(st);
	mmio_state_load(st);
	pad_state_load(st);
	sm83_state_load(st);
	gb_state_load(st);

	/* drop the sound that was generated before the load */
	sound_flush();
}

/* read the picture of the state (the size was checked by the caller) */
static int savestate_read_screen(FILE* f, uint32_t pixels)
{
	uint32_t* screen = lcd_screen_buffer();

	if (!pixels) return 1;
	if (!screen) return 0;
	return fread(screen, 4, pixels, f) == pixels;
}

/* ---------------- slots on disk ---------------- */

int savestate_save(int slot)
{
	char name[64];
	SaveState st;
	FILE* f;
	int ok;

	if (slot < 0 || slot >= SAVESTATE_SLOTS) return 0;

	savestate_collect(&st);
	savestate_file_name(slot, name);

	f = fopen(name, "wb");
	if (!f) {
		__log("save state: cannot write %s", name);
		return 0;
	}

	ok = fwrite(&st, 1, sizeof(st), f) == sizeof(st);
	if (ok && st.ram_size)
		ok = fwrite(cart.ramdata, 1, st.ram_size, f) == st.ram_size;
	if (ok && st.screen_size)
		ok = fwrite(lcd_screen_buffer(), 4, st.screen_size, f) == st.screen_size;
	fclose(f);

	if (!ok) {
		__log("save state: write error on %s", name);
		remove(name);	/* do not leave a half-written slot behind */
		return 0;
	}

	__log("save state: %s (slot %d)", name, slot);
	return 1;
}

int savestate_load(int slot)
{
	char name[64];
	SaveState st;
	FILE* f;
	int status = SS_LOAD_OK;

	if (slot < 0 || slot >= SAVESTATE_SLOTS) return SS_LOAD_BAD;

	savestate_file_name(slot, name);

	f = fopen(name, "rb");
	if (!f) return SS_LOAD_EMPTY;

	if (fread(&st, 1, sizeof(st), f) != sizeof(st))
		status = SS_LOAD_BAD;
	else if (memcmp(st.magic, SAVESTATE_MAGIC, SAVESTATE_MAGIC_LEN) != 0)
		status = SS_LOAD_BAD;
	else if (st.version != SAVESTATE_VERSION || st.state_size != (uint32_t)sizeof(SaveState))
		status = SS_LOAD_BAD;
	else if (st.ram_size != ((cart.ram_size && cart.ramdata) ? cart.ram_size : 0))
		status = SS_LOAD_BAD;
	else if (st.screen_size != (uint32_t)lcd_screen_pixels())
		status = SS_LOAD_BAD;
	else if (st.rom_size != cart.rom_size)
		status = SS_LOAD_OTHER_ROM;
	else if (cart.data && memcmp(st.rom_header, cart.data + 0x100, sizeof(st.rom_header)) != 0)
		status = SS_LOAD_OTHER_ROM;
	else if (st.ram_size && fread(cart.ramdata, 1, st.ram_size, f) != st.ram_size)
		status = SS_LOAD_BAD;
	else if (!savestate_read_screen(f, st.screen_size))
		status = SS_LOAD_BAD;

	fclose(f);

	if (status != SS_LOAD_OK) {
		__log("save state: %s rejected (%d)", name, status);
		return status;
	}

	savestate_apply(&st);
	__log("save state: %s loaded (slot %d)", name, slot);
	return SS_LOAD_OK;
}
