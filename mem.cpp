/* GameBoy memory manager */
#include "pch.h"

mem_Read8P   mem_r8 [256];
mem_Write8P  mem_w8 [256];

/* VRAM */
uint8_t vram[0x2000];

/* internal RAM ($C000-$DFFF) */
uint8_t ram[0x2000];

/* high memory, OAM and hardware registers ($FE00-$FFFF) */
uint8_t hram[0x200];

/***********************************************************************
	memory handlers control
***********************************************************************/


// Map read memory range
void MemMapR(unsigned from,unsigned to,mem_Read8P p) {
	unsigned i;
	for(i=from;i<to;i++) {
		mem_r8[i]=p;
	}
}
// Map write memory range
void MemMapW(unsigned from,unsigned to,mem_Write8P p) {
	unsigned i;
	for(i=from;i<to;i++) {
		mem_w8[i]=p;
	}
}


/***********************************************************************
	Common memory handlers
***********************************************************************/

uint8_t mem_r8_TRAP(unsigned addr) {
	sys_error("Trap on memory read, [%X]",(unsigned)addr);
	return 0xFF;
}
uint8_t mem_r8_emptyROM(unsigned addr){return 0x00; }
uint8_t mem_r8_emptyRAM(unsigned addr) { return 0xFF; }
uint8_t mem_r8_ROMbank0(unsigned addr)
{
	if (addr < 256 && R_BANK == 0) return introm[addr];
	else
	{
		if (cart.data == NULL) return 0xff;
		else return cart.rom[0].ptr[addr];
	}
}
uint8_t mem_r8_ROMbank1(unsigned addr){return cart.rom[1].ptr[addr&0x3FFF];}
uint8_t mem_r8_RAM(unsigned addr) {return ram[addr&0x1FFF];}
uint8_t mem_r8_VRAM(unsigned addr) {return vram[addr&0x1FFF];}
uint8_t mem_r8_RAMbank(unsigned addr) {
	if (cart.ram.ptr) {
		return cart.ram.ptr[addr & cart.ram_amask];
	}
	else {
		return 0xFF;
	}
}
void mem_w8_NULL(unsigned addr, uint8_t n) {}
void mem_w8_TRAP(unsigned addr, uint8_t n) {
	sys_error("Trap on memory write, [%X] <- %X ",(unsigned)addr,(unsigned)n);
}
void mem_w8_VRAM(unsigned addr, uint8_t n) {
	addr &=0x1fff;vram[addr] = n; tilecache[addr>>4] = 0;
}
void mem_w8_RAM(unsigned addr, uint8_t n) {ram[addr&0x1FFF]=n;}
void mem_w8_RAMbank(unsigned addr, uint8_t n) {
	if (cart.ram.ptr) {
		cart.ram.ptr[addr & cart.ram_amask] = n;
	}
}

void SETRAM(unsigned n) {
	n&=cart.ram_nmask;
	//if(n>=cart.ram_nbanks)
		//sys_error("RAM bank not present: [%X]",n);
	cart.ram.bank = n;
	cart.ram.ptr = cart.ramdata+n*0x2000;
}
void SETROM(unsigned n) {
	n&=cart.rom_nmask;
	//if(n>=cart.rom_nbanks)
//		sys_error("ROM bank not present: [%X]",(n));
	cart.rom[1].bank = (n);
	cart.rom[1].ptr = cart.romdata+n*0x4000;
}
// Check for 0 is performed for lower 5 bits, not for whole address


/***********************************************************************
	null MBC :) support
***********************************************************************/

static void InitGenericRAM(void) {
	MEMMAP_W(0x0000,0x2000,mem_w8_MBC1_RAMcontrol);
}

/**********************************************************************
	I/O implementation
***********************************************************************/

uint8_t mem_r8_IO(unsigned addr) {
	__log("HRD %.4X [PC=%.4X]", addr, R_PC);
	if(addr >= 0xff00) {
		if(RANGE(addr, 0xff10, 0xff3f))
			return apu_read((uint8_t)(addr & 0xff));
		switch(addr & 0xff) {
		case 0x00 : 
			{
				uint8_t pad = 0;
				if(R_PAD & 0x20) pad = pad_lo();
				if(R_PAD & 0x10) pad = pad_hi();
				return ~pad & 0xf;
			}
		case 0x4:	// R_DIV - divider counter read
			return (uint8_t)((gb_clk>>6)+gb_divbase);
		case 0x5:	// R_TIMA - timer accumulator read
			if(R_TAC&4)
				return (uint8_t)((gb_clk>>gb_timshift)+gb_timbase); // current value
			// otherwise old(frozen) value will be returned
			break;
		case 0x50:
			return R_BANK & 1;
		}
	}
	return hram[addr & 0x1ff];
}


static const uint8_t timshift[4]={8,2,4,6};

void mem_w8_IO(unsigned addr, uint8_t data) {
	__log("HWR %.4X = %.2X [PC=%.4X]", addr, data, R_PC);

	if(addr>=0xFF00) { // OAM or hram?
		if(RANGE(addr, 0xff10, 0xff3f)) {
			apu_write((uint8_t)(addr & 0xff), data);
			return;
		}
		switch(addr & 0xff) {
/*
		case 0x40: // LCDC
//            if((R_STAT & 3) != 1) return;
			break;
*/
		case 0x04:	// R_DIV - divider counter, reset to 0 when written
			gb_divbase = -(int32_t)(gb_clk >> 6);
			return;
		case 0x05:	// R_TIMA - timer accumulator write
			gb_reload_tima(R_TIMA=data);
			// reload TIMA with new value,calculate time for next interrupt breakpoint
			return;
		case 0x07:  // R_TAC
			if(!(R_TAC^data)) return; // nothing changed
			if(R_TAC&4) // Timer was enabled?
				R_TIMA=(uint8_t)((gb_clk>>gb_timshift)+gb_timbase); // refresh current value
			gb_timshift = timshift[data&3];	// new clock shift rate
			gb_timerclk = MAXULONG;
			if(data&4) // Timer clock enabled?
				gb_reload_tima(R_TIMA); // restart TIMA
			R_TAC=data;
		return;
		case 0x0F : // R_IF  (interrupt request)
			R_IF = data;
			sm83_check4int(); 
		return;
		case 0xFF : // R_IE  (interrupt mask)
			R_IE = data;
			sm83_check4int();
		return;
		//case 0x41: // STAT
		  //  R_STAT = (R_STAT &7)|(data&0xF8);
						//check4LCDint();
//            return;
		case 0x44: R_LY = 0; return;// R_LY reset when written
		case 0x45: R_LYC=data; check4LYC(); // set new LYC, check for interrupt immediately
			return;
			// theoretically LYC interrupt can be caused by setting LYC=LY manually(or not?)
		case 0x46: // DMA
			{
				int i; // TODO: implement proper timing
				uint16_t spraddr = data << 8;
				for(i=0; i<0xa0; i++)
					hram[i]=RD(spraddr + i);
					//WR(0xfe00 | i, RD(spraddr | i));
			}
			break;
		case 0x47: // BGP
			CONVPAL(BGP,data);
			break;
		case 0x48: // OBP0
			CONVPAL(OBP0,data);
			break;
		case 0x49: // OBP1
			CONVPAL(OBP1,data);
			break;
		case 0x50: // BANK
			R_BANK |= (data & 1);		// Write 1 only
			break;
		}
	}
	hram[addr & 0x1ff] = data;
}


/*
*************************************************************************
	memory initialization
*************************************************************************
*/

/* fill zeroed/random data */
static void init_internal_RAM(int how)
{
	int i;

	if(how)
	{
		/* fill by random data */

		for(i=0; i<sizeof(ram); i++)
		{
			if(i % 2) ram[i] = rand() & 0xff;
			else ram[i] = (rand() >> 8) & 0xff;
		}
	}
	else
	{
		/* just clear by zeroes */

		memset(ram, 0, sizeof(ram));
	}

	memset(hram, 0, sizeof(hram));
}

static void mem_InitGeneric(void);

void mem_init()
{
	check_ROM_header();
	init_internal_RAM(1);

	switch(romhdr->type) {
		case 5:
		case 6:
			cart.ram_size = 512;
		break;
	}
	mem_InitGeneric();
	switch(romhdr->type) {
		case 0: // plain ROM only
		break;
		case 8:   // ROM+RAM
		case 9:   // ROM+RAM+BATTERY
			InitGenericRAM();
			break;
		break;
		case 3: // MBC1 RAM+battery
		case 2: // MBC1 RAM
			InitMBC1_RAM();
		case 1: // MBC1
			InitMBC1_ROM();
		break;
		case 6: // MBC2 +battery
		case 5: // MBC2 
			InitMBC2();
		break;
		case 0x10:	// MBC3/RAM/TIMER/BATT
		case 0x12:	// MBC3/RAM
		case 0x13:	// MBC3/RAM/BATT
			InitMBC3_RAM();  //TODO:set ram if only timer present?
		case 0xF:   // MBC3/TIMER is not saved
		case 0x11:  // MBC3
			InitMBC3_ROM();
			break;
		case 0x1A:
		case 0x1B:
		case 0x1D:
		case 0x1E:
			InitMBC5_RAM();
		case 0x19:	// MBC5
		case 0x1C:  // MBC5+rumble
			InitMBC5_ROM();
			break;
		default:
			sys_error("unknown cart type - %X!", romhdr->type);
	}
}

void mem_shutdown()
{
	switch(romhdr->type) {
	case 6:
		if(cart.ram_size>512) cart.ram_size=512;
		break;
	case 0xFF: // HuC-1 
	case 3:    // MBC1 + battery
		if(cart.ram_size>8192) cart.ram_size=8192; // Only 1st page saved
	//case 0xC:
	case 9:    // plain ROM+simple RAM controller+BATTERY
	case 0xD:  // MMM01
	case 0xF:
	case 0x10:
	case 0x13: // MBC3
	case 0x1B:
	case 0x1E: // MBC5
	case 0xFE: // HuC 3
		break;
	default:
		cart.ram_size = 0;
	}
	if(cart.ram_size) {
		save_SRAM(cart.ramdata,cart.ram_size);
	}
	if(cart.ramdata) free(cart.ramdata);
	free(cart.romdata);
}

/* mapper init */

static void mem_InitGeneric(void) {
	unsigned n;
	memset(cart.rom,0,sizeof(cart.rom));
	memset(&cart.ram,0,sizeof(cart.ram));
	cart.romdata = cart.data;
	cart.rom_nbanks = (cart.rom_size+0x3FFF)>>14;
	for(n = 1;n<cart.rom_nbanks;n<<=1);
	cart.rom_nmask = n-1;
	cart.rom[0].ptr = cart.romdata;
	SETROM(1);
	if(cart.ram_size) {
		cart.ram_nbanks = (cart.ram_size+0x1FFF)>>13;
		cart.ram_end= 0xC000; // ram_end is obsolete, whole range is mapped always fo compatibility reasons (ram_amask used instead)
		cart.ram_amask = 0x1FFF;
		if(cart.ram_nbanks==1) {
			cart.ram_end = 0xA000+cart.ram_size;
			for(n=1;n<cart.ram_size;n<<=1);
			cart.ram_amask = n-1;	// set address for small memory sizes
		}
		for(n=1;n<cart.ram_nbanks;n<<=1);
		cart.ram_nmask = n-1;
		cart.ramdata = (uint8_t *)malloc(cart.ram_size);
		load_SRAM(cart.ramdata, cart.ram_size);
		SETRAM(0);
	}
	MEMMAP_R(0x0000,0x10000,mem_r8_TRAP); // first, set traps to whole area
	MEMMAP_W(0x0000,0x10000,mem_w8_TRAP); // no read/write allowed by default

	MEMMAP_W(0x8000,0xA000,mem_w8_VRAM); // video RAM
	MEMMAP_R(0x8000,0xA000,mem_r8_VRAM);
	MEMMAP_W(0xC000,0xE000,mem_w8_RAM);  // internal RAM
	MEMMAP_R(0xC000,0xE000,mem_r8_RAM);
	MEMMAP_W(0xE000,0xFE00,mem_w8_RAM);  // internal RAM mirror
	MEMMAP_R(0xE000,0xFE00,mem_r8_RAM);
	MEMMAP_W(0xFE00,0x10000,mem_w8_IO);  // map IO area
	MEMMAP_R(0xFE00,0x10000,mem_r8_IO);  
	
	MEMMAP_R(0xA000,0xC000,mem_r8_emptyRAM); // RAM is not connected
	MEMMAP_W(0xA000,0xC000,mem_w8_NULL); // RAM is not connected

	MEMMAP_W(0x0000,0x8000,mem_w8_NULL); //
	
	MEMMAP_R(0x0000,0x4000,mem_r8_ROMbank0); // map up to 32 kb of ROM at start
	if (cart.rom_size > 0x4000) {
		MAPROM(mem_r8_ROMbank1);
	}
}

/**********************************************************************
   16 bit read/write subroutines
**********************************************************************/
uint16_t mem_read16 (unsigned addr) {
	return (uint16_t)(
	(unsigned)mem_r8[addr>>8](addr)+
	((unsigned)mem_r8[(addr+1)>>8](addr+1)<<8));
}
void mem_write16 (unsigned addr,unsigned d) {
	mem_w8[addr>>8](addr,(uint8_t)d);
	addr++;
	mem_w8[addr>>8](addr,(uint8_t)(d>>8));
}
