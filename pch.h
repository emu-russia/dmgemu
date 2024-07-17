#pragma once

/* platfrom includes */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#endif
#define SDL_MAIN_HANDLED
#ifdef _WIN32
#include "SDL.h"
#else
#include <SDL2/SDL.h>
#endif
#ifdef _LINUX
#include <unistd.h>
#include <sys/time.h>
#endif

#define MAXULONG (uint32_t)(-1)

/* project includes */
#include "misc.h"
#include "mem.h"
#include "sm83.h"
#include "ppu.h"
#include "lcd.h"
#include "apu.h"
#include "sound.h"
#include "pad.h"
#include "gb.h"
#include "introm.h"
// perftimer-good timer implementation for win32/x86MMX
#include "perftimer.h"
