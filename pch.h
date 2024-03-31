#pragma once

/* platfrom includes */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include <direct.h>

/* platform dependent function marker */
#define PLAT

#define MAXULONG (unsigned long)(-1)

/* Interrupt flags*/
#define INT_NONE	0
#define INT_VBLANK	1
#define INT_LCDSTAT 2
#define INT_TIMER   4
#define INT_SERIAL  8
#define INT_PAD		0x10
#define INT_ALL		0x1F


/* project includes */
#include "misc.h"
#include "mem.h"
#include "sm83.h"
#include "ppu.h"
#include "apu.h"
#include "pad.h"
#include "gb.h"
#include "introm.h"
// perftimer-good timer implementation for win32/x86MMX
#include "perftimer.h"
