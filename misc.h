#pragma once

void sys_error(const char *, ...);
void rand_init();
void load_game(char *);
void show_regs();
void load_SRAM(uint8_t*, long);
void save_SRAM(uint8_t*, long);
void log_init(char *);
void log_shutdown();
void __log(const char *, ...);
