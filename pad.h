#pragma once

void pad_init();
void pad_shutdown();
uint8_t pad_hi();
uint8_t pad_lo();
void pad_sdl_process(SDL_Scancode scan, bool pressed);
