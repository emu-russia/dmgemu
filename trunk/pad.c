/* GameBoy JOY pad emulation */
#include "gbemu.h"

#define VK_A	    0x41
#define VK_S	    0x53
#define VK_X	    0x58
#define VK_Z	    0x5A
#define VK_LEFT	    0x25
#define VK_UP	    0x26
#define VK_RIGHT	0x27
#define VK_DOWN     0x28	

PLAT INTERFACE void pad_init()
{
}

PLAT INTERFACE void pad_shutdown()
{
}

// **********************************************************************

PLAT INTERFACE byte pad_hi()
{
    byte pad = 0;

    if(GetAsyncKeyState(VK_S) & 0x80000000) pad |= 8;      
/* START */
    if(GetAsyncKeyState(VK_A) & 0x80000000) pad |= 4;      
/* SELECT */
    if(GetAsyncKeyState(VK_Z) & 0x80000000) pad |= 2;      
/* B */
    if(GetAsyncKeyState(VK_X) & 0x80000000) pad |= 1;      
/* A */

	if(GetAsyncKeyState(VK_RETURN) & 0x80000000) pad |= 0xf;

    return pad;
}


PLAT INTERFACE byte pad_lo()
{
    byte pad = 0;

    if(GetAsyncKeyState(VK_DOWN) & 0x80000000) pad |= 8;    /* DOWN */
    if(GetAsyncKeyState(VK_UP) & 0x80000000) pad |= 4;      /* UP */
    if(GetAsyncKeyState(VK_LEFT) & 0x80000000) pad |= 2;    /* LEFT */
    if(GetAsyncKeyState(VK_RIGHT) & 0x80000000) pad |= 1;   /* RIGHT */

    return pad;
}