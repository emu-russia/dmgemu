#include "gbemu.h"

#define USEBFLAGS

// This macros serves for switching Z80 undocumented "flags" support off
#ifdef USEBFLAGS
#define SETBF(r) (r&BFLAGS)
#define ADDBF(r) R_F|=(byte)(r&BFLAGS)
#define _SETBF(r) | (r&BFLAGS)
#else
#define SETBF(r)
#define ADDBF(r)
#define _SETBF(r)
#endif

/* �������� GBZ80 */
union Z80reg r_af, r_bc, r_de, r_hl;
union Z80reg r_sp, r_pc;
unsigned HALT, IME;

//int z80_clk = 0;

/*
*************************************************************************
    ������������� GBZ80
*************************************************************************
*/

/* opcode tables */
static void (*opZ80[256])();
static void (*cbZ80[256])();

#define OP(n) break; case 0x##n##:
#define CB(n) break; case 0x##n##:

/* not implemented/reserved instruction */
static void Undefined(void)
{
    R_PC--;
    show_regs ();
    sys_error("Undefined GB Z80 opcode %02X at PC = %.4X",(unsigned)RD(R_PC), (unsigned)(R_PC));
}

static byte z_b_t[256];
static byte inc_t[256];
static byte dec_t[256];
static byte swap_t[256];

// **********************************************************************

/*
*******
    �������
*******
*/

/* _op_ paremeter must be "+=" for DAA and "-=" for DAS operation
 (in Z80 determined by NF flag, in X86 DAS and DAA have different opcodes)
*/
#define _DAA(_op_) {\
		if((tmp32&0xF)>9 || R_F&HF) {\
			tmp32 _op_ 6;\
			tmp8|=(byte)HF;\
		}\
		if(tmp32 > 0x99) {\
			tmp32 _op_ 0x60;\
			tmp8|=(byte)CF;\
}		}


// this _DAS macros added for VGB compatibility, not really needed
#define _DAS(_op_) {\
		if((tmp8=R_F)&HF) {\
			tmp32 _op_ 6;\
			if((signed)tmp32<0) tmp8&=CF;\
		}\
		if(tmp8&CF)\
			tmp32 _op_ 0x60;\
}

#define DAA(x) {\
	tmp32=(unsigned)R_A+((unsigned)(R_F&CF)<<8);\
	if(tmp8=(R_F&NF)) _DAS(-=) else _DAA(+=)\
	R_A = (byte)tmp32;\
	R_F = tmp8 | z_b_t[R_A];\
}


//#define PUSH(n) { R_SP -= 2; WR16(R_SP, n); }
//#define POP(n)  { n = RD16(R_SP); R_SP += 2; }
#define PUSH(n) { R_SP--; WR(R_SP, n.h); R_SP--; WR(R_SP, n.l); }
#define POP(n)  { n.l=RD(R_SP); R_SP++; n.h=RD(R_SP); R_SP++; }


//#define PUSHAF { R_SP -= 2;\
tmp32=R_AF&0xFF00 | (R_F&ZF?0x80:0)|(R_F&NF?0x40:0)|(R_F&HF?0x20:0)|(R_F&CF?0x10:0);\
WR16(R_SP, (word)tmp32); }
//#define POPAF  { tmp32 = RD16(R_SP);\
 tmp32=(tmp32&0xFF00) | (tmp32&0x80?ZF:0)|(tmp32&0x40?NF:0)|(tmp32&0x20?HF:0)|(tmp32&0x10?CF:0);\
 R_AF = (word)tmp32; R_SP += 2; }

// that's conversion to another R_F format, use if needed by any game


#define PUSHAF { R_SP--; WR(R_SP, r_af.l); R_SP--; WR(R_SP, r_af.h); }
#define POPAF  { r_af.h=RD(R_SP); R_SP++; r_af.l=RD(R_SP); R_SP++; }
// high and low part of AF register are swapped in my DMG CPU core


// warning! ADDHL/ADDSPX code is incompatible with 16 bit target cpu
// 8 bit signed value assumed!!!
#define ADDSPX(n)  \
    tmp32 = (unsigned)R_SP + (signed)(signed char)n;\
    R_F = (byte)((tmp32&BFLAGSh | ((signed)(signed char)n^tmp32^R_SP)&HFh)>>8\
		| (tmp32>>16)&CF);

#define LDHLSP(n) { ADDSPX(n);R_HL = (word)tmp32; }
#define ADDSP(n) { ADDSPX(n);R_SP = (word)tmp32; }

// 8 or 16 bit unsigned value assumed
#define ADDHL(n) { \
    tmp32 = (unsigned)R_HL + n; \
	r_af.hl = (word)(r_af.hl&(ZFh|0xFF) | BFLAGSh&tmp32 |\
		HFh & (tmp32^(unsigned)R_HL^n) | (tmp32>>8) & CFh);\
    R_HL = (word)tmp32; }/*T*/

#define ADD(n) { \
    tmp32 = (unsigned)R_A + n;\
    r_af.hl = (word)(((HF & (tmp32^(unsigned)R_A^n))|z_b_t[(byte)tmp32])<<8 | tmp32/*CF*/); \
}

#define ADC(n) { \
    tmp32 = (unsigned)R_A + n + (R_F&CF);\
    r_af.hl = (word)(((HF & (tmp32^(unsigned)R_A^n))|z_b_t[(byte)tmp32])<<8 | tmp32/*CF*/); \
}




// TODO:check    I did just like Faizullin did, don't know if it right
#define CP(n) { \
    tmp32 = (unsigned)R_A - (unsigned)n; \
    R_F   = (byte)((HF & (tmp32^R_A^n)) | -(signed)(tmp32 >> 8)) | z_b_t[(byte)tmp32] | NF;\
}

#define SBC(n) { \
    tmp32 = (unsigned)R_A - (unsigned)n-(R_F&CF); \
    R_F   = (byte)((HF & (tmp32^R_A^n)) | -(signed)(tmp32 >> 8)) | z_b_t[(byte)tmp32] | NF;\
    R_A = (byte)tmp32;\
}

#define SUB(n) { CP(n);R_A = (byte)tmp32;}


#define AND(n) { R_A &= n; R_F = z_b_t[R_A] | HF; }
#define OR(n) { R_A |= n;  R_F = z_b_t[R_A]; }
#define XOR(n) { R_A ^= n; R_F = z_b_t[R_A]; }


#define INC(n) { n++; R_F = (byte)((R_F & CF) | inc_t[n]); }
#define DEC(n) { n--; R_F = (byte)((R_F & CF) | dec_t[n]); }

#define RLCA(r) { \
    r = (byte)(((unsigned)r >> 7) | ((unsigned)r << 1)); \
    R_F = (byte)((R_F&ZF) _SETBF(r) | (r & CF)); }


#define RRCA(r) { \
	R_F = (R_F&ZF) | (byte)(r & CF);\
	r = (byte)(((unsigned)r << 7) | ((unsigned)r >> 1)); \
	ADDBF(r); }
#define RLA(r) { \
    tmp32 = (unsigned)r >> 7; \
    r = (byte)((unsigned)r << 1) | (unsigned)(R_F & CF); \
    R_F = (byte)((R_F&ZF) _SETBF(r) | tmp32);}
#define RRA(r) { \
    tmp32 = (unsigned)R_F<<7; \
	R_F = (byte)((R_F&ZF) | (r & CF));\
	r = (byte)(((unsigned)r >> 1) | tmp32);\
	ADDBF(r);}


#define RLC(r) { \
    r = (byte)(((unsigned)r >> 7) | ((unsigned)r << 1)); \
    R_F = z_b_t[r] | (r & CF); }
#define RRC(r) { \
	R_F = r & CF;\
	r = (byte)(((unsigned)r << 7) | ((unsigned)r >> 1)); \
	R_F |= z_b_t[r]; }
#define RL(r) { \
    tmp32 = (unsigned)r >> 7;/*pos. of CF flag*/ \
    r = (byte)((unsigned)r << 1) | (unsigned)(R_F & CF); \
    R_F = z_b_t[r] | (byte)(tmp32);}
#define RR(r) { \
    tmp32 = (unsigned)R_F<<7; \
	R_F = r & CF;\
	r = (byte)(((unsigned)r >> 1) | tmp32);\
	R_F|=z_b_t[r];}


#define SLA(r) { \
	R_F = (r>>7) & CF;\
    r <<= 1; \
    R_F |= z_b_t[r]; }

#define SRA(r) { \
	R_F = (r & CF);\
    r = (byte)((signed)(signed char)r>>1); \
    R_F |= z_b_t[r]; }


#define SRL(r) { \
	R_F = (r & CF);\
    r >>= 1; \
    R_F |= z_b_t[r]; }


#define BIT(n, r) { R_F = (R_F & CF) | HF| z_b_t[(1<<n) & r]; }
#define RES(n, r) { r &= ~(1 << n); }
#define MRES(n, r) { WR(r, RD(r) & (~(1 << n))); }
#define SET(n, r) { r |= (1 << n); }
#define MSET(n, r) { WR(r, RD(r) | (1 << n)); }

#define RST(n) { PUSH(r_pc); R_PC = n; }

/* **********************************************************************
    clock tables
********/

static int base_clk_t[256] = {
 1, 3, 2, 2, 1, 1, 2, 1, 5, 2, 2, 2, 1, 1, 2, 1,
 1, 3, 2, 2, 1, 1, 2, 1, 3, 2, 2, 2, 1, 1, 2, 1,
 3, 3, 2, 2, 1, 1, 2, 1, 3, 2, 2, 2, 1, 1, 2, 1,
 3, 3, 2, 2, 1, 3, 3, 3, 3, 2, 2, 2, 1, 1, 2, 1,
 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
 2, 2, 2, 2, 2, 2, 1, 2, 1, 1, 1, 1, 1, 1, 2, 1,
 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 1,
 5, 3, 4, 4, 6, 4, 2, 4, 5, 4, 4, 0/*CB*/, 6, 6, 2, 4,
 5, 3, 4, 0, 6, 4, 2, 4, 5, 4, 4, 0, 6, 0, 2, 4,
 3, 3, 2, 0, 0, 4, 2, 4, 4, 1, 4, 0, 0, 0, 2, 4,
 3, 3, 2, 1, 0, 4, 2, 4, 3, 2, 4, 1, 0, 0, 2, 4
};


static int cb_clk_t[256] = {
 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
 2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 3, 2,
 2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 3, 2,
 2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 3, 2,
 2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 2, 2, 3, 2,
 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2,
 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 2, 2, 2, 4, 2
};


/* **********************************************************************
    ����������
********/

//OP(C9);
//OP(CD);



/*
*************************************************************************
    ��������� ��������������
*************************************************************************
*/

void check4int(void) { // Check for posibility of interrupt, do it if possible
	unsigned imask,iflags,intaddr;
    if(iflags=(IME & R_IE & R_IF)) // if interrupts enabled and there are some active interrupts in queue
    {
		imask=1;intaddr=0x40;
		while(!(imask&iflags)) {imask<<=1;intaddr+=8;}
		R_IF &=~imask;	// Clear int request bit
        IME = HALT = 0;	// Disable all interrupts, exit from HALT mode
        PUSH(r_pc);
        R_PC = intaddr;
    }
}
/*
in theory, interrupts must be checked:
- at display status switches(H/V blank, OAM search...) *
- at Vsync *
- at LYC compare(checked at end of h/v blank and at LYC register overwrite) *
- at timer overflow(through separate clock variable) *
- after enabling interrupts(through IRET or IE) *
- after doing manual write to R_IE of R_IF *
*/



/**********************************************************************
   support for code fetching
**********************************************************************/


// TODO: optimize code fetch!!!!!!

static byte fetch(void) {
	unsigned n=R_PC;
	R_PC++;
	return mem_r8[n>>9](n);
}

static word fetch16(void) {
	unsigned n=R_PC;
	R_PC+=2;
	return mem_r8[n>>9](n)+(mem_r8[(n+1)>>9](n+1)<<8);
}

#define FETCH fetch
#define FETCH16 fetch16

#define JR(a) if(R_F & (a)) R_PC += (signed char)FETCH(); else {R_PC++; z80_clk--; }
#define JRN(a) if(!(R_F & (a))) R_PC += (signed char)FETCH(); else {R_PC++; z80_clk--; }



// TODO: check interrupt modes,etc.
/*
Instructions are executed until borderline is reached
borderline can be exceeded my maximum instruction length-1.
borderline typically represent CPU-independent events, such as:
1: LCD mode changes(both LCD interrupts included)
2: timer based interrupts
*/
INTERFACE void gbz80_execute_until(unsigned long clk_nextevent)
{
	/* temporaries for calculations */
byte tmp8;
unsigned tmp32; 
if(HALT) {gb_clk=clk_nextevent;return;}//clkmax;}
while(gb_clk<clk_nextevent
#ifdef VGB_VERIFY
&& !HALT
#endif
	  ) { //TODO: && !HALT move HALT check back after testing
        register unsigned opcode;
		unsigned z80_clk;
#ifdef VGB_VERIFY
	vgb_store();	// Save current state to VGB Z80 core
	debug_canwrite = 0;  // Disable writing to memory/ports
#endif
		opcode = FETCH();
        z80_clk = base_clk_t[opcode];
		switch(opcode) {
case 00: {}    // NOP?
OP(01) { R_BC = FETCH16();}// LD BC,nn
OP(02) { WR(R_BC, R_A); }               // LD (BC),A
OP(03) { R_BC++; }                      // INC BC
OP(04) { INC(R_B); }                    // INC B
OP(05) { DEC(R_B); }                    // DEC B
OP(06) { R_B = FETCH(); }               // LD B,n
OP(07) { RLCA(R_A); }                   // RLCA
OP(08) { tmp32 = FETCH16();WR16(tmp32, R_SP);  }  // [GB] LD (nn),SP
OP(09) { ADDHL(R_BC); }                 // ADD HL,BC
OP(0A) { R_A = RD(R_BC); }              // LD A,(BC)
OP(0B) { R_BC--; }                      // DEC BC
OP(0C) { INC(R_C); }                    // INC C
OP(0D) { DEC(R_C); }                    // DEC C
OP(0E) { R_C = FETCH(); }               // LD C,n
OP(0F) { RRCA(R_A); }                   // RRCA
OP(10) { FETCH();}         // STOP     ? - not in table	
OP(11) { R_DE = FETCH16();}// LD DE,nn
OP(12) { WR(R_DE, R_A); }               // LD (DE),A
OP(13) { R_DE++; }                      // INC DE
OP(14) { INC(R_D); }                    // INC D
OP(15) { DEC(R_D); }                    // DEC D
OP(16) { R_D = FETCH(); }               // LD D,n
OP(17) { RLA(R_A); }					// RLA
OP(18) { R_PC += (signed char)FETCH(); }		// JR PC+n
OP(19) { ADDHL(R_DE); }                 // ADD HL,DE
OP(1A) { R_A = RD(R_DE); }              // LD A,(DE)
OP(1B) { R_DE--; }						// DEC DE
OP(1C) { INC(R_E); }					// INC E
OP(1D) { DEC(R_E); }					// DEC E
OP(1E) { R_E = FETCH(); }				// LD E,n
OP(1F) { RRA(R_A); }					// RRA
OP(20) { JRN(ZF)	}					//JRNZ PC+n
OP(21) { R_HL = FETCH16();}// LD HL,nn
OP(22) { WR(R_HL, R_A); R_HL++; }				// [GB] LD (HL++),A
OP(23) { R_HL++; }						// INC HL
OP(24) { INC(R_H); }					// INC H
OP(25) { DEC(R_H); }					// DEC H
OP(26) { R_H = FETCH(); }				// LD H,n
OP(27) {
	DAA(R_A); }							// DAA
OP(28) { JR(ZF)		}					//JRZ PC+n
OP(29) { ADDHL(R_HL); }					// ADD HL,HL
OP(2A) { R_A = RD(R_HL);R_HL++; }			// [GB] LD A,(HL++)
OP(2B) { R_HL--; }						// DEC HL
OP(2C) { INC(R_L); }					// INC L
OP(2D) { DEC(R_L); }					// DEC L
OP(2E) { R_L = FETCH(); }				// LD L,n
OP(2F) { R_A = ~R_A; R_F = R_F&(CF|ZF) | (R_A&BFLAGS) | (HF|NF); }// CPL
OP(30) { JRN(CF) } //JRNC PC+n
OP(31) { R_SP=FETCH16(); } // LD SP,nn
//R_SP = RD16(R_PC); R_PC += 2; }// LD SP,nn
OP(32) { WR(R_HL, R_A);R_HL--; }				// [GB] LD (HL--),A
OP(33) { R_SP++; }						// INC SP
OP(34) { tmp8 = RD(R_HL); INC(tmp8); WR(R_HL, tmp8); } // INC (HL)
OP(35) {
	tmp8 = RD(R_HL); DEC(tmp8); WR(R_HL, tmp8); } // DEC (HL)
OP(36) { tmp8=FETCH();WR(R_HL, tmp8); }			// LD (HL),n
OP(37) { R_F = (R_F & (byte)~(HF | NF)) | (byte)CF; }	// SCF
OP(38) { JR(CF) } //JRC PC+n
OP(39) { ADDHL(R_SP); }					// ADD HL,(SP)
OP(3A) { R_A = RD(R_HL); R_HL--;}			// [GB] LD A,(HL--)
OP(3B) { R_SP--; }						// DEC SP
OP(3C) { INC(R_A); }					// INC A
OP(3D) { DEC(R_A); }					// DEC A
OP(3E) { R_A = FETCH(); }				// LD A,n
OP(3F) { R_F = ((R_F & (byte)(BFLAGS|ZF|CF)) |
				(R_F&(byte)CF)<<(byte)(HF_POS-CF_POS)) ^ (byte)CF; }// CCF
OP(40) {}								// LD B,B
OP(41) { R_B = R_C; }					// LD B,C
OP(42) { R_B = R_D; }					// LD B,D
OP(43) { R_B = R_E; }					// LD B,E
OP(44) { R_B = R_H; }					// LD B,H
OP(45) { R_B = R_L; }					// LD B,L
OP(46) { R_B = RD(R_HL); }				// LD B,(HL)
OP(47) { R_B = R_A; }					// LD B,A
OP(48) { R_C = R_B; }					// LD C,B
OP(49) {}								// LD C,C
OP(4A) { R_C = R_D; }					// LD C,D
OP(4B) { R_C = R_E; }					// LD C,E
OP(4C) { R_C = R_H; }					// LD C,H
OP(4D) { R_C = R_L; }					// LD C,L
OP(4E) { R_C = RD(R_HL); }				// LD C,(HL)
OP(4F) { R_C = R_A; }					// LD C,A
OP(50) { R_D = R_B; }					// LD D,B
OP(51) { R_D = R_C; }					// LD D,C
OP(52) {}								// LD D,D
OP(53) { R_D = R_E; }					// LD D,E
OP(54) { R_D = R_H; }					// LD D,H
OP(55) { R_D = R_L; }					// LD D,L
OP(56) { R_D = RD(R_HL); }				// LD D,(HL)
OP(57) { R_D = R_A; }					// LD D,A
OP(58) { R_E = R_B; }					// LD E,B
OP(59) { R_E = R_C; }					// LD E,C
OP(5A) { R_E = R_D; }					// LD E,D
OP(5B) {}								// LD E,E
OP(5C) { R_E = R_H; }					// LD E,H
OP(5D) { R_E = R_L; }					// LD E,L
OP(5E) { R_E = RD(R_HL); }				// LD E,(HL)
OP(5F) { R_E = R_A; }					// LD E,A
OP(60) { R_H = R_B; }					// LD H,B
OP(61) { R_H = R_C; }					// LD H,C
OP(62) { R_H = R_D; }					// LD H,D
OP(63) { R_H = R_E; }					// LD H,E
OP(64) {}								// LD H,H
OP(65) { R_H = R_L; }					// LD H,L
OP(66) { R_H = RD(R_HL); }				// LD H,(HL)
OP(67) { R_H = R_A; }					// LD H,A
OP(68) { R_L = R_B; }					// LD L,B
OP(69) { R_L = R_C; }					// LD L,C
OP(6A) { R_L = R_D; }					// LD L,D
OP(6B) { R_L = R_E; }					// LD L,E
OP(6C) { R_L = R_H; }					// LD L,H
OP(6D) {}								// LD L,L
OP(6E) { R_L = RD(R_HL); }				// LD L,(HL)
OP(6F) { R_L = R_A; }					// LD L,A
OP(70) { WR(R_HL, R_B); }				// LD (HL),B
OP(71) { WR(R_HL, R_C); }				// LD (HL),C
OP(72) { WR(R_HL, R_D); }				// LD (HL),D
OP(73) { WR(R_HL, R_E); }				// LD (HL),E
OP(74) { WR(R_HL, R_H); }				// LD (HL),H
OP(75) { WR(R_HL, R_L); }				// LD (HL),L
OP(76) { HALT = 1;
gb_clk = clk_nextevent; // set counters to their end value
#ifndef VGB_VERIFY
return;
#else
break; // In verification mode check must occur before exiting
#endif

}					// HALT
OP(77) { WR(R_HL, R_A); }				// LD (HL),A
OP(78) { R_A = R_B; }					// LD A,B
OP(79) { R_A = R_C; }					// LD A,C
OP(7A) { R_A = R_D; }					// LD A,D
OP(7B) { R_A = R_E; }					// LD A,E
OP(7C) { R_A = R_H; }					// LD A,H
OP(7D) { R_A = R_L; }					// LD A,L
OP(7E) { R_A = RD(R_HL); }				// LD A,(HL)
OP(7F) { }								// LD A,A    (NOP)
OP(80) { ADD(R_B); }					// ADD A,B
OP(81) { ADD(R_C); }					// ADD A,C
OP(82) { ADD(R_D); }					// ADD A,D
OP(83) { ADD(R_E); }					// ADD A,E
OP(84) { ADD(R_H); }					// ADD A,H
OP(85) { ADD(R_L); }					// ADD A,L
OP(86) { tmp8 = RD(R_HL); ADD(tmp8); }	// ADD A,(HL)
OP(87) { ADD(R_A); }					// ADD A,A   (<<1)
OP(88) { ADC(R_B); }					// ADC A,B
OP(89) { ADC(R_C); }					// ADC A,C
OP(8A) { ADC(R_D); }					// ADC A,D
OP(8B) { ADC(R_E); }					// ADC A,E
OP(8C) { ADC(R_H); }					// ADC A,H
OP(8D) { ADC(R_L); }					// ADC A,L
OP(8E) { tmp8 = RD(R_HL); ADC(tmp8); }	// ADC A,(HL)
OP(8F) { ADC(R_A); }					// ADC A,A
OP(90) { SUB(R_B); }					// SUB A,B
OP(91) { SUB(R_C); }					// SUB A,C
OP(92) { SUB(R_D); }					// SUB A,D
OP(93) { SUB(R_E); }					// SUB A,E
OP(94) { SUB(R_H); }					// SUB A,H
OP(95) { SUB(R_L); }					// SUB A,L
OP(96) { tmp8 = RD(R_HL); SUB(tmp8); }	// SUB A,(HL)
OP(97) { SUB(R_A); }					// SUB A,A	- always 0!
OP(98) { SBC(R_B); }					// SBC A,B
OP(99) { SBC(R_C); }					// SBC A,C
OP(9A) { SBC(R_D); }					// SBC A,D
OP(9B) { SBC(R_E); }					// SBC A,E
OP(9C) { SBC(R_H); }					// SBC A,H
OP(9D) { SBC(R_L); }					// SBC A,L
OP(9E) { tmp8 = RD(R_HL); SBC(tmp8); }	// SBC A,(HL)
OP(9F) { SBC(R_A); }					// SBC A,A - value of carry is retained, Z=!C
OP(A0) { AND(R_B); }					// AND A,B
OP(A1) { AND(R_C); }					// AND A,C
OP(A2) { AND(R_D); }					// AND A,D
OP(A3) { AND(R_E); }					// AND A,E
OP(A4) { AND(R_H); }					// AND A,H
OP(A5) { AND(R_L); }					// AND A,L
OP(A6) { AND(RD(R_HL)); }				// AND A,(HL)
OP(A7) { AND(R_A); }					// AND A,A
OP(A8) { XOR(R_B); }					// XOR A,B
OP(A9) { XOR(R_C); }					// XOR A,C
OP(AA) { XOR(R_D); }					// XOR A,D
OP(AB) { XOR(R_E); }					// XOR A,E
OP(AC) { XOR(R_H); }					// XOR A,H
OP(AD) { XOR(R_L); }					// XOR A,L
OP(AE) { XOR(RD(R_HL)); }				// XOR A,(HL)
OP(AF) { XOR(R_A); }					// XOR A,A - =0
OP(B0) { OR(R_B); }						// OR A,B
OP(B1) { OR(R_C); }						// OR A,C
OP(B2) { OR(R_D); }						// OR A,D
OP(B3) { OR(R_E); }						// OR A,E
OP(B4) { OR(R_H); }						// OR A,H
OP(B5) { OR(R_L); }						// OR A,L
OP(B6) { OR(RD(R_HL)); }				// OR A,(HL)
OP(B7) { OR(R_A); }						// OR A,A	A not changed, flags are set
OP(B8) { CP(R_B); }						// CP A,B
OP(B9) { CP(R_C); }						// CP A,C
OP(BA) { CP(R_D); }						// CP A,D
OP(BB) { CP(R_E); }						// CP A,E
OP(BC) { CP(R_H); }						// CP A,H
OP(BD) { CP(R_L); }						// CP A,L
OP(BE) { tmp8 = RD(R_HL); CP(tmp8); }	// CP A,(HL)
OP(BF) { CP(R_A); }						// CP A,A	flags like for 0
OP(C0) { if(!(R_F & ZF))  goto opC9; else z80_clk -= 3; } // RETNZ
OP(C1) { POP(r_bc); }					// POP BC
OP(C2) { if(!(R_F & ZF)) R_PC = FETCH16(); else {R_PC += 2; z80_clk--;} } // JPNZ nn
OP(C3) { R_PC = FETCH16(); }			// JP nn
OP(C4) { if(!(R_F & ZF))  goto opCD; else {R_PC += 2; z80_clk -= 3;} } // CALLNZ
OP(C5) { PUSH(r_bc); }					// PUSH BC
OP(C6) { tmp8 = FETCH(); ADD(tmp8); }	// ADD A,n
OP(C7) { RST(0x00); }					// RST 0
OP(C8) { if(R_F & ZF) goto opC9; else z80_clk -= 3; } // RETZ
OP(C9) 
opC9:
{ POP(r_pc); }					// RET
OP(CA) { if(R_F & ZF) R_PC = FETCH16(); else {R_PC += 2; z80_clk--;} } // JPZ nn
OP(CB) ;
	opcode=FETCH();
	z80_clk = cb_clk_t[opcode]; // TODO:move +1 to table
	//cbZ80[op]();
	switch(opcode) {

case 00: { RLC(R_B); }		// RLC B
CB(01) { RLC(R_C); }		// RLC C
CB(02) { RLC(R_D); }		// RLC D
CB(03) { RLC(R_E); }		// RLC E
CB(04) { RLC(R_H); }		// RLC H
CB(05) { RLC(R_L); }		// RLC L
CB(06) { tmp8 = RD(R_HL);RLC(tmp8);WR(R_HL, tmp8); }	// RLC (HL)
CB(07) { RLC(R_A); }		// RLC A
CB(08) { RRC(R_B); }		// RRC B
CB(09) { RRC(R_C); }		// RLC C
CB(0A) { RRC(R_D); }		// RLC D
CB(0B) { RRC(R_E); }		// RLC E
CB(0C) { RRC(R_H); }		// RLC H
CB(0D) { RRC(R_L); }		// RLC L
CB(0E) { tmp8 = RD(R_HL);RRC(tmp8);WR(R_HL, tmp8); }	// RRC (HL)
CB(0F) { RRC(R_A); }		// RLC A
CB(10) { RL(R_B); }			// RL B
CB(11) { RL(R_C); }			// RL C
CB(12) { RL(R_D); }			// RL D
CB(13) { RL(R_E); }			// RL E
CB(14) { RL(R_H); }			// RL H
CB(15) { RL(R_L); }			// RL L
CB(16) { tmp8 = RD(R_HL);RL(tmp8);WR(R_HL, tmp8); }	// RL (HL)
CB(17) { RL(R_A); }			// RL A
CB(18) { RR(R_B); }			// RR B
CB(19) { RR(R_C); }			// RR C
CB(1A) { RR(R_D); }			// RR D
CB(1B) { RR(R_E); }			// RR E
CB(1C) { RR(R_H); }			// RR H
CB(1D) { RR(R_L); }			// RR L
CB(1E) { tmp8 = RD(R_HL);RR(tmp8);WR(R_HL, tmp8); }	// RR (HL)
CB(1F) { RR(R_A); }			// RR A
CB(20) { SLA(R_B); }		// SLA B
CB(21) { SLA(R_C); }		// SLA C
CB(22) { SLA(R_D); }		// SLA D
CB(23) { SLA(R_E); }		// SLA E
CB(24) { SLA(R_H); }		// SLA H
CB(25) { SLA(R_L); }		// SLA L
CB(26) { tmp8 = RD(R_HL);SLA(tmp8);WR(R_HL, tmp8); }	// SLA (HL)
CB(27) { SLA(R_A); }		// SLA A
CB(28) { SRA(R_B); }		// SRA B
CB(29) { SRA(R_C); }		// SRA C
CB(2A) { SRA(R_D); }		// SRA D
CB(2B) { SRA(R_E); }		// SRA E
CB(2C) { SRA(R_H); }		// SRA H
CB(2D) { SRA(R_L); }		// SRA L
CB(2E) { tmp8 = RD(R_HL);SRA(tmp8);WR(R_HL, tmp8); }	// SRA (HL)
CB(2F) { SRA(R_A); }		// SRA A
CB(30) { R_B = swap_t[R_B];}// R_F = z_b_t[R_B]; }  // [GB] SWAP B
CB(31) { R_C = swap_t[R_C];}// R_F = z_b_t[R_C]; }  // [GB] SWAP C
CB(32) { R_D = swap_t[R_D];}// R_F = z_b_t[R_D]; }  // [GB] SWAP D
CB(33) { R_E = swap_t[R_E];}// R_F = z_b_t[R_E]; }  // [GB] SWAP E
CB(34) { R_H = swap_t[R_H];}// R_F = z_b_t[R_H]; }  // [GB] SWAP H
CB(35) { R_L = swap_t[R_L];}// R_F = z_b_t[R_L]; }  // [GB] SWAP L
CB(36) { WR(R_HL,tmp8=swap_t[RD(R_HL)]);}// R_F = z_b_t[tmp8];; }  // [GB] SWAP (HL)
CB(37) { R_A = swap_t[R_A];}// R_F = z_b_t[R_A]; }  // [GB] SWAP A
CB(38) { SRL(R_B); }		// SRL B
CB(39) { SRL(R_C); }		// SRL C
CB(3A) { SRL(R_D); }		// SRL D
CB(3B) { SRL(R_E); }		// SRL E
CB(3C) { SRL(R_H); }		// SRL H
CB(3D) { SRL(R_L); }		// SRL L
CB(3E) { tmp8 = RD(R_HL);SRL(tmp8);WR(R_HL, tmp8); }	// SRL (HL)
CB(3F) { SRL(R_A); }		// SRL A
CB(40) { BIT(0, R_B); }	
CB(41) { BIT(0, R_C); }
CB(42) { BIT(0, R_D); }
CB(43) { BIT(0, R_E); }
CB(44) { BIT(0, R_H); }
CB(45) { BIT(0, R_L); }
CB(46) { BIT(0, RD(R_HL)); }
CB(47) { BIT(0, R_A); }
CB(48) { BIT(1, R_B); }
CB(49) { BIT(1, R_C); }
CB(4A) { BIT(1, R_D); }
CB(4B) { BIT(1, R_E); }
CB(4C) { BIT(1, R_H); }
CB(4D) { BIT(1, R_L); }
CB(4E) { BIT(1, RD(R_HL)); }
CB(4F) { BIT(1, R_A); }
CB(50) { BIT(2, R_B); }
CB(51) { BIT(2, R_C); }
CB(52) { BIT(2, R_D); }
CB(53) { BIT(2, R_E); }
CB(54) { BIT(2, R_H); }
CB(55) { BIT(2, R_L); }
CB(56) { BIT(2, RD(R_HL)); }
CB(57) { BIT(2, R_A); }
CB(58) { BIT(3, R_B); }
CB(59) { BIT(3, R_C); }
CB(5A) { BIT(3, R_D); }
CB(5B) { BIT(3, R_E); }
CB(5C) { BIT(3, R_H); }
CB(5D) { BIT(3, R_L); }
CB(5E) { BIT(3, RD(R_HL)); }
CB(5F) { BIT(3, R_A); }
CB(60) { BIT(4, R_B); }
CB(61) { BIT(4, R_C); }
CB(62) { BIT(4, R_D); }
CB(63) { BIT(4, R_E); }
CB(64) { BIT(4, R_H); }
CB(65) { BIT(4, R_L); }
CB(66) { BIT(4, RD(R_HL)); }
CB(67) { BIT(4, R_A); }
CB(68) { BIT(5, R_B); }
CB(69) { BIT(5, R_C); }
CB(6A) { BIT(5, R_D); }
CB(6B) { BIT(5, R_E); }
CB(6C) { BIT(5, R_H); }
CB(6D) { BIT(5, R_L); }
CB(6E) { BIT(5, RD(R_HL)); }
CB(6F) { BIT(5, R_A); }
CB(70) { BIT(6, R_B); }
CB(71) { BIT(6, R_C); }
CB(72) { BIT(6, R_D); }
CB(73) { BIT(6, R_E); }
CB(74) { BIT(6, R_H); }
CB(75) { BIT(6, R_L); }
CB(76) { BIT(6, RD(R_HL)); }
CB(77) { BIT(6, R_A); }
CB(78) { BIT(7, R_B); }
CB(79) { BIT(7, R_C); }
CB(7A) { BIT(7, R_D); }
CB(7B) { BIT(7, R_E); }
CB(7C) { BIT(7, R_H); }
CB(7D) { BIT(7, R_L); }
CB(7E) { BIT(7, RD(R_HL)); }
CB(7F) { BIT(7, R_A); }
CB(80) { RES(0, R_B); }
CB(81) { RES(0, R_C); }
CB(82) { RES(0, R_D); }
CB(83) { RES(0, R_E); }
CB(84) { RES(0, R_H); }
CB(85) { RES(0, R_L); }
CB(86) { MRES(0, R_HL); }
CB(87) { RES(0, R_A); }
CB(88) { RES(1, R_B); }
CB(89) { RES(1, R_C); }
CB(8A) { RES(1, R_D); }
CB(8B) { RES(1, R_E); }
CB(8C) { RES(1, R_H); }
CB(8D) { RES(1, R_L); }
CB(8E) { MRES(1, R_HL); }
CB(8F) { RES(1, R_A); }
CB(90) { RES(2, R_B); }
CB(91) { RES(2, R_C); }
CB(92) { RES(2, R_D); }
CB(93) { RES(2, R_E); }
CB(94) { RES(2, R_H); }
CB(95) { RES(2, R_L); }
CB(96) { MRES(2, R_HL); }
CB(97) { RES(2, R_A); }
CB(98) { RES(3, R_B); }
CB(99) { RES(3, R_C); }
CB(9A) { RES(3, R_D); }
CB(9B) { RES(3, R_E); }
CB(9C) { RES(3, R_H); }
CB(9D) { RES(3, R_L); }
CB(9E) { MRES(3, R_HL); }
CB(9F) { RES(3, R_A); }
CB(A0) { RES(4, R_B); }
CB(A1) { RES(4, R_C); }
CB(A2) { RES(4, R_D); }
CB(A3) { RES(4, R_E); }
CB(A4) { RES(4, R_H); }
CB(A5) { RES(4, R_L); }
CB(A6) { MRES(4, R_HL); }
CB(A7) { RES(4, R_A); }
CB(A8) { RES(5, R_B); }
CB(A9) { RES(5, R_C); }
CB(AA) { RES(5, R_D); }
CB(AB) { RES(5, R_E); }
CB(AC) { RES(5, R_H); }
CB(AD) { RES(5, R_L); }
CB(AE) { MRES(5, R_HL); }
CB(AF) { RES(5, R_A); }
CB(B0) { RES(6, R_B); }
CB(B1) { RES(6, R_C); }
CB(B2) { RES(6, R_D); }
CB(B3) { RES(6, R_E); }
CB(B4) { RES(6, R_H); }
CB(B5) { RES(6, R_L); }
CB(B6) { MRES(6, R_HL); }
CB(B7) { RES(6, R_A); }
CB(B8) { RES(7, R_B); }
CB(B9) { RES(7, R_C); }
CB(BA) { RES(7, R_D); }
CB(BB) { RES(7, R_E); }
CB(BC) { RES(7, R_H); }
CB(BD) { RES(7, R_L); }
CB(BE) { MRES(7, R_HL); }
CB(BF) { RES(7, R_A); }
CB(C0) { SET(0, R_B); }
CB(C1) { SET(0, R_C); }
CB(C2) { SET(0, R_D); }
CB(C3) { SET(0, R_E); }
CB(C4) { SET(0, R_H); }
CB(C5) { SET(0, R_L); }
CB(C6) { MSET(0, R_HL); }
CB(C7) { SET(0, R_A); }
CB(C8) { SET(1, R_B); }
CB(C9) { SET(1, R_C); }
CB(CA) { SET(1, R_D); }
CB(CB) { SET(1, R_E); }
CB(CC) { SET(1, R_H); }
CB(CD) { SET(1, R_L); }
CB(CE) { MSET(1, R_HL); }
CB(cf) { SET(1, R_A); }
CB(D0) { SET(2, R_B); }
CB(D1) { SET(2, R_C); }
CB(D2) { SET(2, R_D); }
CB(D3) { SET(2, R_E); }
CB(D4) { SET(2, R_H); }
CB(D5) { SET(2, R_L); }
CB(D6) { MSET(2, R_HL); }
CB(D7) { SET(2, R_A); }
CB(D8) { SET(3, R_B); }
CB(D9) { SET(3, R_C); }
CB(DA) { SET(3, R_D); }
CB(DB) { SET(3, R_E); }
CB(DC) { SET(3, R_H); }
CB(DD) { SET(3, R_L); }
CB(DE) { MSET(3, R_HL); }
CB(DF) { SET(3, R_A); }
CB(E0) { SET(4, R_B); }
CB(E1) { SET(4, R_C); }
CB(E2) { SET(4, R_D); }
CB(E3) { SET(4, R_E); }
CB(E4) { SET(4, R_H); }
CB(E5) { SET(4, R_L); }
CB(E6) { MSET(4, R_HL); }
CB(E7) { SET(4, R_A); }
CB(E8) { SET(5, R_B); }
CB(E9) { SET(5, R_C); }
CB(EA) { SET(5, R_D); }
CB(EB) { SET(5, R_E); }
CB(EC) { SET(5, R_H); }
CB(ED) { SET(5, R_L); }
CB(EE) { MSET(5, R_HL); }
CB(EF) { SET(5, R_A); }
CB(F0) { SET(6, R_B); }
CB(F1) { SET(6, R_C); }
CB(F2) { SET(6, R_D); }
CB(F3) { SET(6, R_E); }
CB(F4) { SET(6, R_H); }
CB(F5) { SET(6, R_L); }
CB(F6) { MSET(6, R_HL); }
CB(F7) { SET(6, R_A); }
CB(F8) { SET(7, R_B); }
CB(F9) { SET(7, R_C); }
CB(FA) { SET(7, R_D); }
CB(FB) { SET(7, R_E); }
CB(FC) { SET(7, R_H); }
CB(FD) { SET(7, R_L); }
CB(FE) { MSET(7, R_HL); }
CB(FF) { SET(7, R_A); }
break;
}   /* select CB table */
OP(CC) { if(R_F & ZF) goto opCD; else {R_PC += 2; z80_clk -= 3;} } // CALLZ
OP(CD) 
opCD:
{ tmp32 = FETCH16();///RD16(R_PC);R_PC+=2;
PUSH(r_pc);
R_PC = (word)tmp32; }		// CALL
OP(CE) { tmp8 = FETCH(); ADC(tmp8); }		// ADC A,n
OP(cf) { RST(0x08); }						// RST 8
OP(D0) { if(!(R_F & CF)) goto opC9; else z80_clk -= 3; } // RETNC
OP(D1) { POP(r_de); }
OP(D2) { if(!(R_F & CF)) R_PC = FETCH16(); else {R_PC += 2; z80_clk--;} }
OP(D3) {Undefined();}			// [GB] NOT implemented
OP(D4) { if(!(R_F & CF)) goto opCD; else {R_PC += 2; z80_clk -= 3;} }
OP(D5) { PUSH(r_de); }					// PUSH DE
OP(D6) { tmp8 = FETCH(); SUB(tmp8); }	// SUB A,n
OP(D7) { RST(0x10); }					// RST 10h
OP(D8) { if(R_F & CF) goto opC9; else z80_clk -= 3; } // RETC
OP(D9) { POP(r_pc); IME = INT_ALL;
#ifndef VGB_VERIFY
check4int();
#endif
}			// [GB]	IRET
OP(DA) { if(R_F & CF) R_PC = FETCH16(); else {R_PC += 2; z80_clk--;} } // JPC nn
OP(DB) {Undefined();}			// [GB] not implemented
OP(DC) { if(R_F & CF) goto opCD; else {R_PC += 2; z80_clk -= 3;} } // CALLC
OP(DD) {Undefined();}			// [GB] not implemented
OP(DE) { tmp8 = FETCH(); SBC(tmp8); }	// SBC A,n
OP(DF) { RST(0x18); }					// RST 18h
OP(E0) {
	tmp32=0xff00 + FETCH();WR(tmp32, R_A); }	// [GB] LD (FF00+n),A
OP(E1) { POP(r_hl); }					// POP HL
OP(E2) { tmp32=0xff00 + R_C; WR(tmp32, R_A); }		// [GB] LD (FF00+C),A
OP(E3) {Undefined();}			// [GB] NOT implemented
OP(E4) {Undefined();}			// [GB] NOT implemented
OP(E5) { PUSH(r_hl); }					// PUSH HL
OP(E6) { tmp8=FETCH();AND(tmp8); }				// AND A,n
OP(E7) { RST(0x20); }					// RST 20h
OP(E8) { tmp8 = FETCH(); ADDSP(tmp8); }	// ADD SP,n 
OP(E9) { R_PC = R_HL; }					// JP (HL)
OP(EA) { tmp32 = FETCH16();/*RD16(R_PC);R_PC += 2*/WR(tmp32, R_A); } // [GB] LD (nn),A   TODO: unchecked. FETCH32?
OP(EB) {Undefined();}			// [GB] NOT implemented (but should be)
OP(EC) {Undefined();}			// [GB] NOT implemented
OP(ED) {Undefined();}			// [GB] NOT implemented ?????????? (former prefix)
OP(EE) { tmp8=FETCH();XOR(tmp8); }				// XOR A,n
OP(EF) { RST(0x28); }					// RST 28h
OP(F0) { tmp32 = 0xff00 + FETCH(); R_A = RD(tmp32); }	// [GB] LD A,(FF00+n)
OP(F1) { POPAF; }					// POP AF
OP(F2) { tmp32 = 0xff00 + R_C; R_A = RD(tmp32); }		// [GB] LD A,(FF00+C)
OP(F3) { IME = INT_NONE; }						// DI   (=CLI)
OP(F4) {Undefined();}			// [GB] NOT implemented
OP(F5) { PUSHAF; }					// PUSH AF
OP(F6) { tmp8=FETCH();OR(tmp8); }					// OR A,n
OP(F7) { RST(0x30); }					// RST 30h
OP(F8) { tmp8 = FETCH(); LDHLSP(tmp8); }// [GB] LDHL SP,n  (LEA HL,[SP+n])
OP(F9) { R_SP = R_HL; }					// LD SP,HL
OP(FA) { 
	tmp32 = FETCH16();R_A = RD(tmp32); }// [GB] LD A,(nn)
OP(FB) { IME = INT_ALL;
#ifndef VGB_VERIFY
check4int();
#endif
}						// EI   (=STI)
OP(FC) {Undefined();}			// [GB] NOT implemented
OP(FD) {Undefined();}			// [GB] NOT implemented
OP(FE) { tmp8 = FETCH(); CP(tmp8); }	// CP A,n
OP(FF) { RST(0x38); }					// RST 38h
			break;
		}
		gb_clk+=z80_clk;	// Increment core clock counter
		//so_clk+=z80_clk;	// Increment sound clock counter
/* TODO: both counters can be same, if wrapping for internal sound counters is moved
         to gb.c. As result one slow memory->memory addition will be removed
*/
		//__log("Op %.2X",opcode);

#ifdef VGB_VERIFY
		debug_canwrite = 1;	// Enable writing
		vgb_compare();		// Execute one instruction in VGB Z80 core, compare results
		check4int();		// Check for interrupt
#endif
    }
}

// **********************************************************************

static void filltables(void) {
	unsigned i,p;
	for(i=0;i<256;i++) {
		swap_t[i] = (byte)((i<<4)|(i>>4));
		p = i&BFLAGS;
		if(!i) p|=ZF;
		z_b_t[i]=p;
		inc_t[i] = (i&0xF)==0x0?  (p|HF)  : p;
		p|=NF;
		dec_t[i] = (i&0xF)==0xF?  (p|HF)  : p;
	}
}
INTERFACE void gbz80_init()
{
	filltables();  // This makes code more readable, editable & compressable :)
    R_PC = 0;
#ifdef VGB_VERIFY
	vgbz80_init();
#endif
    HALT = IME = 0;
}
