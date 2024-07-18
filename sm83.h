#pragma once

// If the build is for processors like PowerPC (e.g. GameCube), then activate this macro.
//#define BIGENDIAN

#pragma pack(push, 1)
union Z80reg {
	struct {
#ifdef BIGENDIAN
		uint8_t h, l;
#else
		uint8_t l, h;
#endif
	};
	uint16_t hl;
	unsigned align; // So 1 Z80reg instance will be always equal to CPU word size
};
#pragma pack(pop)

extern union Z80reg r_af, r_bc, r_de, r_hl;
extern union Z80reg r_sp, r_pc;

extern unsigned HALT, IME;

#define R_AF r_af.hl
#define R_BC r_bc.hl
#define R_DE r_de.hl
#define R_HL r_hl.hl
#define R_SP r_sp.hl
#define R_PC r_pc.hl

#define R_A r_af.h
#define R_F r_af.l
#define R_B r_bc.h
#define R_C r_bc.l
#define R_D r_de.h
#define R_E r_de.l
#define R_H r_hl.h
#define R_L r_hl.l

/* flags */

#define ZF_POS 7
#define NF_POS 6
#define HF_POS 5
#define CF_POS 4

#define ZF (uint8_t)(1<<ZF_POS)
#define NF (uint8_t)(1<<NF_POS)
#define HF (uint8_t)(1<<HF_POS)
#define CF (uint8_t)(1<<CF_POS)

/* CPU interface */
void sm83_init();
void sm83_execute_until(uint32_t clk_nextevent); // Exact timing is not guarantied
void sm83_check4int(void); // Call interrupt if IE,request flags are set and not masked.
