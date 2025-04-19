#pragma once

/* ROM header */
struct rom_header_t {
	uint8_t		code[4];
	uint16_t    logo[24];
	char		title[16];
	char		rsrv[3];
	uint8_t		type, romsize, ramsize;
	uint8_t		country, licensee;
	uint8_t		version;
	uint8_t		header_chk;
	uint16_t	global_chk;
};

extern rom_header_t* romhdr;

struct MemBank {
	unsigned flags;
	unsigned bank;
	uint8_t* ptr;
	unsigned z0;
};

struct Cartridge {
	uint8_t* data; // ROM data
	uint8_t* romdata; // ROM data
	uint8_t* ramdata; // RAM data
	unsigned rom_size;	// Size of ROM in bytes
	unsigned ram_size;  // Size of RAM in bytes
	unsigned rom_nbanks;   // Size of ROM in banks (mapper dependent)
	unsigned ram_nbanks;   // Size of RAM in banks (mapper dependent)
	unsigned rom_nmask;	   // Mask for ROM bank selection
	unsigned ram_nmask;  // Mask for RAM bank selection
	unsigned ram_end;	   // End address for usable RAM area in CPU address space
	unsigned ram_amask;	   // Mask for allowed RAM bank space 

	MemBank rom[4]; // selected ROM bank(s)
	MemBank ram[4]; // selected RAM bank(s)

	char title[20];
};

extern Cartridge cart;

void check_ROM_header();

void mem_w8_MBC1_RAMcontrol(unsigned addr, uint8_t n);

void InitMBC1_ROM(void);
void InitMBC1_RAM(void);
void InitMBC2(void);
void InitMBC3_ROM(void);
void InitMBC3_RAM(void);
void InitMBC5_ROM(void);
void InitMBC5_RAM(void);