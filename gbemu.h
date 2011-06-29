/* platfrom includes */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* module interface function marker */
#define INTERFACE

/* platform dependent function marker */
#define PLAT

/* GB related types */
typedef unsigned char byte;
typedef unsigned short word;
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
#include "gbz80.h"
#include "lcd.h"
#include "so.h"
#include "pad.h"
#include "gb.h"
