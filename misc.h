#pragma once

/* Enough room for a full path plus a suffix, see save_file_name() */
#define SAVE_FILE_NAME_MAX	512

void sys_error(const char *, ...);
void rand_init();
void load_game(char *);

/* <path of the loaded ROM without extension><suffix>, e.g. "D:\Roms\DMG\BTDD.sav".
   The cartridge title must not be used as a file name: it may contain
   characters that are illegal in a path (BTDD.gb is titled "B_TOADS/D_DRAGON"). */
void save_file_name(char *out, int out_size, const char *suffix);
void show_regs();
void load_SRAM(uint8_t*, long);
void save_SRAM(uint8_t*, long);
void log_init(char *);
void log_shutdown();
void __log(const char *, ...);
