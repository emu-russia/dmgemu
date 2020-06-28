/* ROM header */
typedef struct {
	uint8_t		code[4];
	uint16_t    logo[24];
	char		title[16];
	char		rsrv[3];
	uint8_t		type, romsize, ramsize;
	uint8_t		country, licensee;
	uint8_t		version;
	uint8_t		header_chk;
	uint16_t	global_chk;
} rom_header_t;

extern rom_header_t *romhdr;

extern uint8_t vram[0x2000];

extern uint8_t hram[0x200];


// Gameboy Color palettes
#define R_BCPS hram[0x100 + 0x68]
// BG palette index (0-0x1F)
#define R_BCPD hram[0x100 + 0x69]
// BG palette data
#define R_OCPS hram[0x100 + 0x6A]
// OBJ palette index (0-0x1F)
#define R_OCPD hram[0x100 + 0x6B]
// OBJ palette data


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

#define R_INTROM hram[0x100 + 0x50]

#define HRAM(addr) hram[0x100 + (addr & 0xff)]
#define RANGE(x, a, b) ((x >= a) && (x <= b))

struct MemBank {
	unsigned flags;
	unsigned bank;
	uint8_t*ptr;
	unsigned z0;
};

extern struct Cartridge {
	uint8_t*data; // ROM data
	uint8_t*romdata; // ROM data
	uint8_t*ramdata; // RAM data
	unsigned rom_size;	// Size of ROM in bytes
	unsigned ram_size;  // Size of RAM in bytes
	unsigned rom_nbanks;   // Size of ROM in banks (mapper dependent)
	unsigned ram_nbanks;   // Size of RAM in banks (mapper dependent)
	unsigned rom_nmask;	   // Mask for ROM bank selection
	unsigned ram_nmask;  // Mask for RAM bank selection
	unsigned ram_end;	   // End address for usable RAM area in Z80 address space
	unsigned ram_amask;	   // Mask for allowed RAM bank space 

	struct MemBank rom[4]; // selected ROM bank(s)
	struct MemBank ram[4]; // selected RAM bank(s)

	char title[20];
} cart;

void mem_init();
void mem_shutdown();

uint16_t mem_read16 (unsigned addr);
void mem_write16 (unsigned addr,unsigned d);


typedef uint8_t(mem_Read8)(unsigned);
typedef void (mem_Write8)(unsigned, uint8_t);
typedef mem_Read8 *mem_Read8P;
typedef mem_Write8 *mem_Write8P;

extern mem_Read8P   mem_r8 [128];
extern mem_Write8P  mem_w8 [128];
//extern mem_read16P  mem_r16[128];
//extern mem_write16P mem_w16[128];


// There is no checks for allowed read.

#define RD(n) mem_r8[(unsigned)(n)>>9]((unsigned)(n))
//#define WR(n, d) {unsigned t=n;byte dd=d;if(debug_canwrite) mem_w8[(unsigned)(t)>>9]((unsigned)(t),dd);}
#define WR(n, d) mem_w8[(unsigned)(n)>>9]((unsigned)(n),(uint8_t)(d));

#define RD16(n) mem_read16(n)
//#define WR16(n, d) {unsigned t=n;word dd=d;if(debug_canwrite) mem_write16(t,dd);}
#define WR16(n, d) mem_write16(n,d)


