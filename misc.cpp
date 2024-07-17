/* misc. platform stuff */
#include "pch.h"

void sys_error(const char *fmt, ...)
{
	va_list	arg;
	char	buf[0x1000];

	va_start(arg, fmt);
	vsprintf(buf, fmt, arg);
	va_end(arg);

#ifdef _WIN32
	MessageBox(NULL, buf, "SYSTEM ERROR", MB_OK | MB_TOPMOST | MB_ICONSTOP);
#else
	printf("SYSTEM ERROR: %s\n", buf);
#endif
	exit(1);
}

void rand_init()
{
	srand(time(0));
}

/* load game from file */
void load_game(char *name)
{
	FILE *f;
	long size;

	f = fopen(name, "rb");
	if (!f) {
		sys_error("couldn't load game \'%s\'", name);
		return;
	}

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	memset(&cart,0,sizeof(cart));
	cart.data = (uint8_t *)malloc(size);
	if (!cart.data) {
		fclose(f);
		sys_error("not enough memory for game!");
		return;
	}

	int readed = fread(cart.data, 1, size, f);
	if (readed != size) {
		__log ("Did not read as many bytes as specified!");
	}
	fclose(f);
}

/* use only for DEBUG */
void show_regs()
{
	char buf[0x1000]{};
	int i, p = 0;

	p += sprintf(&buf[p], "AF=%.4X\t\tBC=%.4X\t\tDE=%.4X\nHL=%.4X\n", R_AF, R_BC, R_DE, R_HL);
	p += sprintf(&buf[p], "SP=%.4X\t\tPC=%.4X\t\tCLK=%d\n\n", R_SP, R_PC, (int)gb_clk);

	p += sprintf(&buf[p], "P1=%.2X\t\tTIMA=%.2X\t\tIF=%.2X\n", HRAM(0xff00), HRAM(0xff05), HRAM(0xff0f));
	p += sprintf(&buf[p], "SB=%.2X\t\tTMA=%.2X\t\tIE=%.2X\n", HRAM(0xff01), HRAM(0xff06), HRAM(0xffff));
	p += sprintf(&buf[p], "SC=%.2X\t\tTAC=%.2X\n", HRAM(0xff02), HRAM(0xff07));
	p += sprintf(&buf[p], "DIV=%.2X\n\n", HRAM(0xff04));

	p += sprintf(&buf[p], "NR10=%.2X\t\tNR21=%.2X\t\tNR30=%.2X\n", HRAM(0xff10), HRAM(0xff16), HRAM(0xff1a));
	p += sprintf(&buf[p], "NR11=%.2X\t\tNR22=%.2X\t\tNR31=%.2X\n", HRAM(0xff11), HRAM(0xff17), HRAM(0xff1b));
	p += sprintf(&buf[p], "NR12=%.2X\t\tNR23=%.2X\t\tNR32=%.2X\n", HRAM(0xff12), HRAM(0xff18), HRAM(0xff1c));
	p += sprintf(&buf[p], "NR13=%.2X\t\tNR24=%.2X\t\tNR33=%.2X\n", HRAM(0xff13), HRAM(0xff19), HRAM(0xff1d));
	p += sprintf(&buf[p], "NR14=%.2X\t\t\t\tNR34=%.2X\n\n", HRAM(0xff14), HRAM(0xff1e));

	p += sprintf(&buf[p], "NR41=%.2X\t\tNR50=%.2X\n", HRAM(0xff20), HRAM(0xff24));
	p += sprintf(&buf[p], "NR42=%.2X\t\tNR51=%.2X\n", HRAM(0xff21), HRAM(0xff25));
	p += sprintf(&buf[p], "NR43=%.2X\t\tNR52=%.2X\n", HRAM(0xff22), HRAM(0xff26));
	p += sprintf(&buf[p], "NR40=%.2X\n\n", HRAM(0xff23));

	p += sprintf(&buf[p], "WAVE:\n");
	for(i=0; i<16; i++)
		p += sprintf(&buf[p], "%.2X ", HRAM(0xff30 + i));
	p += sprintf(&buf[p], "\n\n");

	p += sprintf(&buf[p], "LCDC=%.2X\tLY=%.2X\t\tBGP=%.2X\n", HRAM(0xff40), HRAM(0xff44), HRAM(0xff47));
	p += sprintf(&buf[p], "STAT=%.2X \tLYC=%.2X\t\tOBP0=%.2X\n", HRAM(0xff41), HRAM(0xff45), HRAM(0xff48));
	p += sprintf(&buf[p], "SCX=%.2X\t\tWX=%.2X\t\tOBP1=%.2X\n", HRAM(0xff43), HRAM(0xff4b), HRAM(0xff49));
	p += sprintf(&buf[p], "SCY=%.2X\t\tWY=%.2X\n", HRAM(0xff42), HRAM(0xff4a));

#ifdef _WIN32
	MessageBox(NULL, buf, "GB SM83 and hardware register map", MB_OK | MB_TOPMOST | MB_ICONINFORMATION);
#else
	printf("GB SM83 and hardware register map: %s", buf);
#endif
}

/* load battery-backed/onboard RAM */
void load_SRAM(uint8_t* ram_ptr, long size)
{
	FILE *f;
	long i;
	char name[128];

	sprintf(name, "%s.sav", romhdr->title);
	f = fopen(name, "rb");
	if(!f)
	{
		/* no support for BATTERY, yet ;) */
		for(i=0; i<size; i++)
			ram_ptr[i] = rand() & 0xff;
		return;
	}

	int readed = fread(ram_ptr, 1, size, f);
	if (readed != size) {
		__log ("Did not read as many bytes as specified!");
	}
	fclose(f);
}

/* save battery-backed/onboard RAM */
void save_SRAM(uint8_t* ram_ptr, long size)
{
	FILE *f;
	char name[128];

	sprintf(name, "%s.sav", romhdr->title);
	f = fopen(name, "wb");
	if(!f) return;
	fwrite(ram_ptr, 1, size, f);
	fclose(f);
}

/*
*************************************************************************
	GB process logging
*************************************************************************
*/

static FILE *__log__ = NULL;

void log_init(char *file)
{
	__log__ = fopen(file, "w");
	if(!__log__) return;
}

void log_shutdown()
{
	if (__log__) {
		fclose(__log__);
		__log__ = nullptr;
	}
}

void __log(const char *fmt, ...)
{
	va_list	arg;
	char buf[0x1000]{};

	if(__log__)
	{
		va_start(arg, fmt);
		vsprintf(buf, fmt, arg);
		va_end(arg);

		fprintf(__log__, "%s\n", buf);
	}
}
