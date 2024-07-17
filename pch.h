#pragma once

/* platfrom includes */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include <direct.h>
#include "SDL.h"

#define MAXULONG (unsigned long)(-1)

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
