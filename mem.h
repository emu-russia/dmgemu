#pragma once

struct SZstruct {
	int n;
	int sz;
};

extern uint8_t vram[0x2000];

extern uint8_t hram[0x200];


#define R_PAD   hram[0x100 + 0x00]

#define R_DIV   hram[0x100 + 0x04]
#define R_TIMA  hram[0x100 + 0x05]
#define R_TMA   hram[0x100 + 0x06]
#define R_TAC   hram[0x100 + 0x07]


#define R_LCDC  hram[0x100 + 0x40]
#define R_STAT  hram[0x100 + 0x41]
#define R_SCY   hram[0x100 + 0x42]
#define R_SCX   hram[0x100 + 0x43]
#define R_LY    hram[0x100 + 0x44]
#define R_LYC   hram[0x100 + 0x45]
#define R_DMA   hram[0x100 + 0x46]
#define R_BGP   hram[0x100 + 0x47]
#define R_OBP0  hram[0x100 + 0x48]
#define R_OBP1  hram[0x100 + 0x49]
#define R_WY    hram[0x100 + 0x4a]
#define R_WX    hram[0x100 + 0x4b]

#define R_IF    hram[0x100 + 0x0f]
#define R_IE    hram[0x100 + 0xff]

#define R_NR10  hram[0x100 + 0x10]
#define R_NR11  hram[0x100 + 0x11]
#define R_NR12  hram[0x100 + 0x12]
#define R_NR13  hram[0x100 + 0x13]
#define R_NR14  hram[0x100 + 0x14]
#define R_NR21  hram[0x100 + 0x16]
#define R_NR22  hram[0x100 + 0x17]
#define R_NR23  hram[0x100 + 0x18]
#define R_NR24  hram[0x100 + 0x19]
#define R_NR30  hram[0x100 + 0x1a]
#define R_NR31  hram[0x100 + 0x1b]
#define R_NR32  hram[0x100 + 0x1c]
#define R_NR33  hram[0x100 + 0x1d]
#define R_NR34  hram[0x100 + 0x1e]
#define R_NR41  hram[0x100 + 0x20]
#define R_NR42  hram[0x100 + 0x21]
#define R_NR43  hram[0x100 + 0x22]
#define R_NR44  hram[0x100 + 0x23]
#define R_NR50  hram[0x100 + 0x24]
#define R_NR51  hram[0x100 + 0x25]
#define R_NR52  hram[0x100 + 0x26]

#define R_BANK  hram[0x100 + 0x50]

#define HRAM(addr) hram[0x100 + (addr & 0xff)]
#define RANGE(x, a, b) ((x >= a) && (x <= b))

void mem_init();
void mem_shutdown();

uint16_t mem_read16 (unsigned addr);
void mem_write16 (unsigned addr,unsigned d);


typedef uint8_t(mem_Read8)(unsigned);
typedef void (mem_Write8)(unsigned, uint8_t);
typedef mem_Read8 *mem_Read8P;
typedef mem_Write8 *mem_Write8P;

extern mem_Read8P   mem_r8 [256];
extern mem_Write8P  mem_w8 [256];

void MemMapR(unsigned from, unsigned to, mem_Read8P p);
void MemMapW(unsigned from, unsigned to, mem_Write8P p);


#define MEMMAP_W(a,b,p) MemMapW((a)>>8,(b)>>8,p)
#define MEMMAP_R(a,b,p) MemMapR((a)>>8,(b)>>8,p)
#define MAPROM(x) MEMMAP_R(0x4000,0x8000,x);
#define MAPRAM(r,w) {MEMMAP_R(0xA000,0xC000/*cart.ram_end*/,r);MEMMAP_W(0xA000,0xC000,w);}

void SETRAM(unsigned n);
void SETROM(unsigned i, unsigned n);

uint8_t mem_r8_emptyROM(unsigned addr);
uint8_t mem_r8_emptyRAM(unsigned addr);
uint8_t mem_r8_ROMbank1(unsigned addr);
uint8_t mem_r8_RAMbank(unsigned addr);
void mem_w8_RAMbank(unsigned addr, uint8_t n);
void mem_w8_NULL(unsigned addr, uint8_t n);

// There is no checks for allowed read.

#define RD(n) mem_r8[(unsigned)(n)>>8]((unsigned)(n))
#define WR(n, d) mem_w8[(unsigned)(n)>>8]((unsigned)(n),(uint8_t)(d));

#define RD16(n) mem_read16(n)
#define WR16(n, d) mem_write16(n,d)