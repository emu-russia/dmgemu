/* GameBoy memory manager */
#include "pch.h"

#define Kbit *128
#define Mbit *(128*1024)

typedef struct SZstruct_ {
	int n;
	int sz;
} SZstruct;

static SZstruct ROM_SIZES[] = {
{0,  256 Kbit},
{1,  512 Kbit},
{2,  1   Mbit},
{3,  2   Mbit},
{4,  4   Mbit},
{5,  8   Mbit},
{6,  16  Mbit},
{7,  32  Mbit},
{8,  64  Mbit},
{52, 9   Mbit},
{53, 10  Mbit},
{54, 12  Mbit},
{64, 20  Mbit},
{65, 24  Mbit},
{74, 36  Mbit},
{75, 40  Mbit},
{76, 48  Mbit},
{-1,0}};

static SZstruct RAM_SIZES[] = {
{0,0   Kbit},
{1,16  Kbit},
{2,64  Kbit},
{3,256 Kbit},
{4,1   Mbit},
{-1,0}};

#define BANK_ENABLED 1

struct Cartridge cart;

mem_Read8P   mem_r8 [128];
mem_Write8P  mem_w8 [128];

/* ROM banks */
//uint8_t *rom0, *rom1;
//uint8_t trash[0x4000];
 
/* VRAM */
uint8_t vram[0x2000];

/* current RAM bank and enable flag */
//uint8_t *ram_bank;
//int ram_bank_enable;

/* internal RAM ($C000-$DFFF) */
uint8_t ram[0x2000];

/* high memory, OAM and hardware registers ($FE00-$FFFF) */
uint8_t hram[0x200];

extern mem_Read8P   mem_r8 [128];
extern mem_Write8P  mem_w8 [128];

//void (*memini[6])();

/***********************************************************************
	memory handlers control
***********************************************************************/


// Map read memory range
static void MemMapR(unsigned from,unsigned to,mem_Read8P p) {
	unsigned i;
	for(i=from;i<to;i++) {
		mem_r8[i]=p;
	}
}
// Map write memory range
static void MemMapW(unsigned from,unsigned to,mem_Write8P p) {
	unsigned i;
	for(i=from;i<to;i++) {
		mem_w8[i]=p;
	}
}
#define MEMMAP_W(a,b,p) MemMapW((a)>>9,(b)>>9,p)
#define MEMMAP_R(a,b,p) MemMapR((a)>>9,(b)>>9,p)



/***********************************************************************
	Common memory handlers
***********************************************************************/

uint8_t mem_r8_TRAP(unsigned addr) {
	sys_error("Trap on memory read, [%X]",(unsigned)addr);
	return 0xFF;
}
uint8_t mem_r8_emptyROM(unsigned addr){return 0x00; }
uint8_t mem_r8_ROMbank0(unsigned addr)
{
	if (addr < 256 && R_INTROM == 0) return introm[addr];
	else
	{
		if (cart.data == NULL) return 0xff;
		else return cart.rom[0].ptr[addr];
	}
}
uint8_t mem_r8_ROMbank1(unsigned addr){return cart.rom[1].ptr[addr&0x3FFF];}
uint8_t mem_r8_RAM(unsigned addr) {return ram[addr&0x1FFF];}
uint8_t mem_r8_VRAM(unsigned addr) {return vram[addr&0x1FFF];}
uint8_t mem_r8_RAMbank(unsigned addr) {return cart.ram[0].ptr[addr&cart.ram_amask];}
uint8_t mem_r8_RAMbank4(unsigned addr) {return cart.ram[0].ptr[addr&0x1FF]|0xF0;}
void mem_w8_NULL(unsigned addr, uint8_t n) {}
void mem_w8_TRAP(unsigned addr, uint8_t n) {
	sys_error("Trap on memory write, [%X] <- %X ",(unsigned)addr,(unsigned)n);
}
void mem_w8_VRAM(unsigned addr, uint8_t n) {
	addr &=0x1fff;vram[addr] = n; tilecache[addr>>4] = 0;
}
//void mem_w8_RAM(unsigned addr,uint8_t n) {ram[addr&0x1FFF]=n;}
void mem_w8_RAM(unsigned addr, uint8_t n) {ram[addr&0x1FFF]=n;}
void mem_w8_RAMbank(unsigned addr, uint8_t n) {cart.ram[0].ptr[addr&cart.ram_amask]=n;}
void mem_w8_RAMbank4(unsigned addr, uint8_t n) {cart.ram[0].ptr[addr&0x1FF]=n;}

#define MAPROM(x) MEMMAP_R(0x4000,0x8000,x);

#define MAPRAM(r,w) {MEMMAP_R(0xA000,0xC000/*cart.ram_end*/,r);MEMMAP_W(0xA000,0xC000,w);}

/*#define SETRAM(rb) if(rb>=cart.ram_nbanks) \
			sys_error("RAM bank not present: [%X]",(rb));\
			cart.ram[0].bank = rb;\
		cart.ram[0].ptr = cart.ramdata+(rb)*0x4000;*/
static void SETRAM(unsigned n) {
	n&=cart.ram_nmask;
	//if(n>=cart.ram_nbanks)
		//sys_error("RAM bank not present: [%X]",n);
	cart.ram[0].bank = n;
	cart.ram[0].ptr = cart.ramdata+n*0x4000;
}
static void SETROM(unsigned n) {
	n&=cart.rom_nmask;
	//if(n>=cart.rom_nbanks)
//		sys_error("ROM bank not present: [%X]",(n));
	cart.rom[1].bank = (n);
	cart.rom[1].ptr = cart.romdata+n*0x4000;
}
// Check for 0 is performed for lower 5 bits, not for whole address


//#define SETROM1(n) if(!(n)) (n)++; SETROM(n);
/***********************************************************************
	MBC1 support
***********************************************************************/
static struct MBC_ {
	unsigned mode2;	// MBC1
	unsigned ramenabled;  // All MBCs
	unsigned clock_present; // MBC3
	unsigned clock_latched; // MBC3
} MBC;
// mode1 - 32*4 pages of ROM, 1 page of RAM
// mode2 - 32 pages of ROM, 1*4 pages of RAM

void mem_w8_MBC1_RAMcontrol(unsigned addr, uint8_t n) {
	if((n&0xF) == 0xA) {
		if(!MBC.ramenabled) MAPRAM(mem_r8_RAMbank,mem_w8_RAMbank);
		MBC.ramenabled = 1;
	} else {
		if(MBC.ramenabled) MAPRAM(mem_r8_emptyROM,mem_w8_NULL);
		MBC.ramenabled = 0;
	}
}
void mem_w8_MBC1_setROM(unsigned addr, uint8_t n) {
	unsigned nn=n&0x1F; // 5 bits
	if(!nn) nn++;
	if(!MBC.mode2) nn|=cart.rom[1].bank&(3<<5);  //
	SETROM(nn);
}
void mem_w8_MBC1_setROMorRAM(unsigned addr, uint8_t n) {
	unsigned nn=(unsigned)n&3; // 2 bits
	if(MBC.mode2) {
		if(cart.ram_nbanks) SETRAM(nn);
	} else {
		nn=(nn<<5)+(cart.rom[1].bank&0x1F);  //
		SETROM(nn);
	}
}
void mem_w8_MBC1_mode(unsigned addr, uint8_t n) {
	unsigned nn;
	if(MBC.mode2 == (n&(unsigned)1)) return;
	MBC.mode2 = n&(unsigned)1;
	nn = cart.rom[1].bank&0x1F;
	if(!nn) nn=1;
	SETROM(nn);
	if(cart.ram_nbanks) SETRAM(0);
}

static void InitMBC1_ROM(void) {
	MEMMAP_W(0x2000,0x4000,mem_w8_MBC1_setROM);
	MEMMAP_W(0x4000,0x6000,mem_w8_MBC1_setROMorRAM);
	MEMMAP_W(0x6000,0x8000,mem_w8_MBC1_mode);
	MAPROM(mem_r8_ROMbank1);
	mem_w8_MBC1_mode(0,0);
	MBC.ramenabled = 0;
}

static void InitMBC1_RAM(void) {
	MEMMAP_W(0x0000,0x2000,mem_w8_MBC1_RAMcontrol);
}

/***********************************************************************
	null MBC :) support
***********************************************************************/

static void InitGenericRAM(void) {
	MEMMAP_W(0x0000,0x2000,mem_w8_MBC1_RAMcontrol);
}

/***********************************************************************
	MBC2 support
***********************************************************************/


// TODO: I don't know exactly how to map those 512x4 bits properly.
void mem_w8_MBC2_set(unsigned addr, uint8_t n) {
	unsigned nn=n&0xF;
	if(addr&0x2100) {  // ROM control
		if(!nn) nn=1;
		SETROM(nn);
	} else {		  // RAM control
		if(nn == 0xA) {
			if(!MBC.ramenabled) MAPRAM(mem_r8_RAMbank,mem_w8_RAMbank);
			MBC.ramenabled = 1;
		} else {
			if(MBC.ramenabled) MAPRAM(mem_r8_emptyROM,mem_w8_NULL);
			MBC.ramenabled = 0;
		}
	}
}

static void InitMBC2(void) {
	MEMMAP_W(0x0000,0x4000,mem_w8_MBC2_set);
	MAPROM(mem_r8_ROMbank1);
	MAPRAM(mem_r8_emptyROM,mem_w8_NULL);
	SETRAM(0);
	MBC.ramenabled = 0;
}

/***********************************************************************
	MBC3 support
***********************************************************************/
// TODO: no realtime clock support

void mem_w8_MBC3_setROM(unsigned addr, uint8_t n) {
	//register unsigned nn=n&0x7F; // 7 bits
	//if(!MBC.mode2) nn|=cart.rom[1].bank&~0x1F;  //
	if(!n) n = 1;
	SETROM(n);
}
void mem_w8_MBC3_setRAM_or_clock(unsigned addr, uint8_t n) {
	if(!(n&~3)) { // set RAM bank
		if(cart.ram_nbanks) SETRAM(n);
	} else {	// set RT clock register
		
	}
}
void mem_w8_MBC3_clocklatch(unsigned addr, uint8_t n) {
	
}

static void InitMBC3_ROM(void) {
	MEMMAP_W(0x2000,0x4000,mem_w8_MBC3_setROM);
	MEMMAP_W(0x4000,0x6000,mem_w8_MBC3_setRAM_or_clock);
	MEMMAP_W(0x6000,0x8000,mem_w8_MBC3_clocklatch);
	MAPROM(mem_r8_ROMbank1);
	MBC.ramenabled = 0;
}

static void InitMBC3_RAM(void) {
	InitMBC1_RAM();
}

/***********************************************************************
	MBC5 support
***********************************************************************/

void mem_w8_MBC5_setROM0(unsigned addr, uint8_t n) {
	unsigned nn=(unsigned)n|(cart.rom[1].bank&0x100);
	SETROM(nn);
}
void mem_w8_MBC5_setROM1(unsigned addr, uint8_t n) {
	unsigned nn=((unsigned)n<<8)|(cart.rom[1].bank&0xFF);
	SETROM(nn);
}
void mem_w8_MBC5_setRAM(unsigned addr, uint8_t n) {
	SETRAM(n);
}

static void InitMBC5_ROM(void) {
	MEMMAP_W(0x2000,0x3000,mem_w8_MBC5_setROM0);
	MEMMAP_W(0x3000,0x4000,mem_w8_MBC5_setROM1);
	MEMMAP_W(0x4000,0x6000,mem_w8_MBC5_setRAM);
	//MEMMAP_W(0x6000,0x8000,mem_w8_MBC3_clocklatch);
	MAPROM(mem_r8_ROMbank1);
	MBC.ramenabled = 0;
}

static void InitMBC5_RAM(void) {
	InitMBC1_RAM();
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
				uint8_t pad;
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
		}
	}
	return hram[addr & 0x1ff];
}

#define CONVPAL(to,data) \
	to[0] = (data >> (0 << 1)) & 3;\
	to[1] = (data >> (1 << 1)) & 3;\
	to[2] = (data >> (2 << 1)) & 3;\
	to[3] = (data >> (3 << 1)) & 3;


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
			gb_divbase = -(signed long)(gb_clk >> 6);
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
			check4int(); 
		return;
		case 0xFF : // R_IE  (interrupt mask)
			R_IE = data;
			check4int();
		return;
		/*case 0x10: case 0x11: case 0x12: case 0x13:
		case 0x14: case 0x15: case 0x16: case 0x17:
		case 0x18: case 0x19: case 0x1A: case 0x1B:
		case 0x1C: case 0x1D: case 0x1E: case 0x1F:
		case 0x20: case 0x21: case 0x22: case 0x23:
		case 0x24: case 0x25: case 0x26: case 0x27:
		case 0x28: case 0x29: case 0x2A: case 0x2B:
		case 0x2C: case 0x2D: case 0x2E: case 0x2F:
		case 0x30: case 0x31: case 0x32: case 0x33:
		case 0x34: case 0x35: case 0x36: case 0x37:
		case 0x38: case 0x39: case 0x3A: case 0x3B:
		case 0x3C: case 0x3D: case 0x3E: case 0x3F:
			so_write((uint8_t)(addr & 0xff), data);
		return;*/
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
		}
	}
	hram[addr & 0x1ff] = data;
}


/*
*************************************************************************
	memory initialization
*************************************************************************
*/

rom_header_t *romhdr;

static void do_header_chk(int fire)
{
	uint8_t *buf = (uint8_t *)romhdr->title;
	int chk = 0;
	int i;

	for(i=0; i<25; i++)
		chk += buf[i];

	chk = 0x100 - (chk + 25);

	if(fire && romhdr->header_chk != (uint8_t)chk)
		sys_error("header checksum failed!");
}

/*static void do_global_chk(int fire)
{
	long i;
	register u_long chk = 0;

	for(i=0; i<rom_size; i++)
	{
		if(i != 0x14e && i != 0x14f)
			chk += game[i];
	}

	chk &= 0xffff;
	chk = (chk << 8) | (chk >> 8);

	if(fire && romhdr->global_chk != (uint16_t)chk)
		sys_error("global checksum failed!");
}*/


static int findsz(unsigned *sz,SZstruct *from,int what) {
	 while(2*2 == 4) {
		if(from->n == -1) return 0;
		if(from->n == what) {
			*sz = from->sz;
			return -1;
		}
		from++;
	};
}

static void check_ROM_header()
{
	if (cart.data == NULL)
	{
		static rom_header_t empty;
		romhdr = &empty;
		empty.type = 0;
		strcpy(cart.title,"NO CARD");
		return;
	}
	romhdr = (rom_header_t *)(cart.data+0x100);
	if(!findsz(&(cart.rom_size),ROM_SIZES,romhdr->romsize))
		sys_error("Unknown ROM size");
	if(!findsz(&(cart.ram_size),RAM_SIZES,romhdr->ramsize))
		sys_error("Unknown RAM size");
	do_header_chk(0);
	memcpy(cart.title,romhdr->title,15);
//    do_global_chk(1);
}

/* only for DEBUG */
static void dump_internal_RAM()
{
#if 0
	FILE *f;

	f = fopen("RAM.bin", "wb");
	fwrite(ram, 1, sizeof(ram), f);
	fclose(f);

	f = fopen("HRAM.bin", "wb");
	fwrite(hram, 1, sizeof(hram), f);
	fclose(f);
#endif
}

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

	//dump_internal_RAM();
}

void mem_InitGeneric(void);

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
	memset(cart.ram,0,sizeof(cart.ram));
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

	//MEMMAP_R(0xA000,0xC000,mem_r8_emptyROM);
	MEMMAP_W(0xA000,0xC000,mem_w8_NULL); // RAM is not connected

	MEMMAP_W(0x0000,0x8000,mem_w8_NULL); //
	

	//if((n=cart.rom_size)>0x8000) n = 0x8000;
	MEMMAP_R(0x0000,0x4000,mem_r8_ROMbank0); // map up to 32 kb of ROM at start
	if(cart.rom_size>0x4000)
		MAPROM(mem_r8_ROMbank1);
		//MEMMAP_R(0xA000,0xA000+n,mem_r8_emptyROM); // RAM is disabled by default
	//MEMMAP_R(0xA000,0xA000+n,mem_r8_RAMbank); // map up to 8 kb of RAM at start
}

/**********************************************************************
   16 bit read/write subroutines
**********************************************************************/
uint16_t mem_read16 (unsigned addr) {
	return (uint16_t)(
	(unsigned)mem_r8[addr>>9](addr)+
	((unsigned)mem_r8[(addr+1)>>9](addr+1)<<8));
}
void mem_write16 (unsigned addr,unsigned d) {
	mem_w8[addr>>9](addr,(uint8_t)d);
	addr++;
	mem_w8[addr>>9](addr,(uint8_t)(d>>8));
}
