/* GameBoy JOY pad emulation */
#include "pch.h"

static uint8_t pad_override[2];

void pad_init()
{
	pad_override[0] = 0;
	pad_override[1] = 0;
}

void pad_shutdown()
{
}

// **********************************************************************

uint8_t pad_hi()
{
	return pad_override[0];
}

uint8_t pad_lo()
{
	return pad_override[1];
}

void pad_sdl_process(SDL_Scancode scan, bool pressed)
{
	switch (scan) {
		case SDL_SCANCODE_S:	/* START */
			if (pressed) pad_override[0] |= 8;
			else pad_override[0] &= ~8;
			break;
		case SDL_SCANCODE_A:	/* SELECT */
			if (pressed) pad_override[0] |= 4;
			else pad_override[0] &= ~4;
			break;
		case SDL_SCANCODE_Z:	/* B */
			if (pressed) pad_override[0] |= 2;
			else pad_override[0] &= ~2;
			break;
		case SDL_SCANCODE_X:	/* A */
			if (pressed) pad_override[0] |= 1;
			else pad_override[0] &= ~1;
			break;

		case SDL_SCANCODE_RETURN:	/* All Buttons */
			if (pressed) pad_override[0] |= 0xf;
			else pad_override[0] &= ~0xf;
			break;

		case SDL_SCANCODE_DOWN:	/* DOWN */
			if (pressed) pad_override[1] |= 8;
			else pad_override[1] &= ~8;
			break;
		case SDL_SCANCODE_UP:	/* UP */
			if (pressed) pad_override[1] |= 4;
			else pad_override[1] &= ~4;
			break;
		case SDL_SCANCODE_LEFT:	/* LEFT */
			if (pressed) pad_override[1] |= 2;
			else pad_override[1] &= ~2;
			break;
		case SDL_SCANCODE_RIGHT:	/* RIGHT */
			if (pressed) pad_override[1] |= 1;
			else pad_override[1] &= ~1;
			break;
	}
}