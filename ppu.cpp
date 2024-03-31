/* GameBoy LCD emulation (win32) */
#include "pch.h"

unsigned lcd_WYline;
int lcd_fpslimit=1;
int lcd_effect=1; // possible values: 0,1

//static uint8_t lcd_user_screen[144][160];

unsigned mainpal[64];  // 0-3 BG palette 4-7,8-11 - sprite palettes
static uint8_t linebuffer[192]; // showed from 8-th byte


/*
*************************************************************************
	platform crap (win32) - DIB-sections
*************************************************************************
*/

/*
**  how to use all this stuff :
**  ~~~~~~~~~~~~~~~~~~~~~~~~~~~
**
**  INITIALIZATION :
**
**      - call win32_win_init(), to create main window.
**      - center main window by WIN_Center() (if you want).
**      - call win32_dib_init() to properly create DIB-section.
**
**  USAGE :
**      - "pbuf" holds pointer to "window background". you can think,
**          that it is your videobuffer and you can draw to it directly.
**      - call lcd_refresh() to refresh window, after drawing.
**      - and don't foreget to provide all messages to window procedure 
**          by win32_win_update().
*/

PLAT unsigned benchmark_sound,benchmark_gfx;

#define WIN_STYLE ( WS_CAPTION | WS_BORDER | WS_SYSMENU )

#define SCALE 2

//#define GFX8BIT



/*static */HWND main_hwnd;
static HDC main_hdc, hdcc;
static HBITMAP DIB_section;
static HGDIOBJ old_obj;
//static RGBQUAD *pbuf;

#ifdef GFX8BIT
uint8_t *pbuf;
#else
RGBQUAD *pbuf;
#endif

/* milk to cofee */
RGBQUAD dib_pal[] = {
	{ 143, 231, 255 },   // color #0 (milk)
	{ 95 , 176, 223 },   // color #1
	{ 63 , 120, 144 },   // color #2
	{ 31 , 56 , 79  }    // color #3 (cofee)
};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static int so_enabled = 1;

	switch(msg)
	{
		case WM_CREATE:
			break;
		case WM_CLOSE:
			DestroyWindow (hwnd);
			break;
		case WM_DESTROY:
			gb_shutdown ();
			exit (0);
			break;
		case WM_KEYDOWN:
			switch(wParam) 
			{ 
				case VK_ESCAPE: DestroyWindow(hwnd); break;
				case VK_F12 :
					(so_enabled) ? so_shutdown() : so_init(44100);
					so_enabled ^= 1;
					break;
				case VK_F8:
					lcd_fpslimit^=1;
				break;
				case VK_F9:
					lcd_effect^=1; // possible values: 0,1
				break;
			}
			break;

		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

static void win32_win_init(int width, int height)
{
	HINSTANCE hInstance = GetModuleHandle(NULL);
	char title[128];
	WNDCLASS wc;
	RECT rect;
	int w, h;

	sprintf(title, "GameBoy - %s", romhdr->title);

	wc.cbClsExtra = wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = "LCDWIN";
	wc.lpszMenuName = NULL;
	wc.style = 0;

	if(!RegisterClass(&wc))
		sys_error("couldn't register main window class.");

	rect.left = 0;
	rect.top = 0;
	rect.right = width * SCALE;
	rect.bottom = height * SCALE;

	AdjustWindowRect(&rect, WIN_STYLE, 0);

	w = rect.right - rect.left;
	h = rect.bottom - rect.top;

	main_hwnd = CreateWindow(
		"LCDWIN", title,
		WIN_STYLE, 
		CW_USEDEFAULT, CW_USEDEFAULT,
		w, h,
		NULL, NULL,
		hInstance, NULL);

	if(!main_hwnd)
		sys_error("couldn't create main window.");

	ShowWindow(main_hwnd, SW_NORMAL);
	UpdateWindow(main_hwnd);
}

static void win32_win_update()
{
	MSG msg;

	if(PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if(msg.message == WM_QUIT)
		{
			gb_shutdown();
			exit(0);
		}
		if(GetMessage(&msg, NULL, 0, 0))
			DispatchMessage(&msg);
	}
}

static void WIN_Center(HWND hwnd)
{
	WINDOWPLACEMENT pos;
	HDC display;
	int w, h, x, y;
	RECT rect;
	int win_width, win_height;

	GetWindowRect(hwnd, &rect);
	win_width = rect.right - rect.left;
	win_height = rect.bottom - rect.top;

	display = CreateDC("DISPLAY", NULL, NULL, NULL);
	w = GetDeviceCaps(display, HORZRES);
	h = GetDeviceCaps(display, VERTRES);
	DeleteDC(display);

	pos.length = sizeof(WINDOWPLACEMENT);
	GetWindowPlacement(hwnd, &pos);
	x = w/2 - win_width/2;
	y = h/2 - win_height/2;
	pos.ptMinPosition.x = x;
	pos.ptMinPosition.y = y;
	pos.ptMaxPosition.x = x;
	pos.ptMaxPosition.y = y;
	pos.rcNormalPosition.top = y;
	pos.rcNormalPosition.bottom = y + win_height;
	pos.rcNormalPosition.left = x;
	pos.rcNormalPosition.right = x + win_width;
	SetWindowPlacement(hwnd, &pos);
}

int dib_width, dib_height;

static void win32_dib_init(int width, int height)
{
	HDC hdc;
	BITMAPINFO *bmi;
	void *DIB_base;

	bmi=(BITMAPINFO*)calloc(sizeof(BITMAPINFO)+16*4,1);
	main_hdc = hdc = GetDC(main_hwnd);

	//memset(&(bmi->bmiHeader), 0, sizeof(BITMAPINFOHEADER));

	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	dib_width = bmi->bmiHeader.biWidth = width;
	dib_height = bmi->bmiHeader.biHeight = -height;
	bmi->bmiHeader.biPlanes = 1;
#ifdef GFX8BIT
	bmi->bmiHeader.biBitCount = 8;
	bmi->bmiHeader.biClrUsed = bmi->bmiHeader.biClrImportant =4;
#else
	bmi->bmiHeader.biBitCount = 32;
#endif
	bmi->bmiHeader.biCompression = BI_RGB;
	//bmi->bmiHeader.

	DIB_section = CreateDIBSection(hdc, bmi, DIB_RGB_COLORS, &DIB_base, NULL, 0);
#ifdef GFX8BIT
	pbuf = DIB_base;
#else
	pbuf = (RGBQUAD *)DIB_base;
#endif

	hdcc = CreateCompatibleDC(hdc);
	if(!hdcc)
		sys_error("CreateCompatibleDC() failed!");

	if(!(old_obj = SelectObject(hdcc, DIB_section)))
		sys_error("SelectObject() failed!");
#ifdef GFX8BIT
	if(!(SetDIBColorTable(hdcc,0,4,dib_pal)))
		sys_error("SetDIBColorTable() failed!");
#endif
	free(bmi);
}

static void win32_dib_shutdown()
{
	if(hdcc)
	{
		SelectObject(hdcc, old_obj);
		DeleteDC(hdcc);
	}
	if(DIB_section) DeleteObject(DIB_section);
	if(main_hdc) ReleaseDC(main_hwnd, main_hdc);
}

static void lcd_refresh(int line)
{
	int i;
#ifdef GFX8BIT
	register unsigned char *p = (unsigned char *)pbuf+160 * line;
	//memcpy(pbuf+160*line,(linebuffer+8)+i,160);
	for(i=0; i<160; i++) p[i] = mainpal[(linebuffer+8)[i]&0x3F];
#else
	register unsigned long *p = (unsigned long *)pbuf+160 * line;
	if(lcd_effect==1)
	for(i=0; i<160; i++)
		p[i] = (0x7F7F7F&(p[i]>>1))+(0x7F7F7F&(((unsigned long*)dib_pal)[mainpal[(linebuffer+8)[i]&0x3F]]>>1));
	else
	for(i=0; i<160; i++)
		p[i] = ((unsigned long*)dib_pal)[mainpal[(linebuffer+8)[i]&0x3F]];
#endif
}

/*
*************************************************************************
	LCD interface
*************************************************************************
*/

void tilecache_init(void);

PLAT void lcd_init() 
{
	tilecache_init();
	//memset(lcd_user_screen, 0, sizeof(lcd_user_screen));

	win32_win_init(160, 144);
	WIN_Center(main_hwnd);
	win32_dib_init(160, 144);
	lcd_WYline=-1;
}

PLAT void lcd_shutdown()
{
	win32_dib_shutdown();
}

// **********************************************************************

typedef struct {
	uint8_t    y, x, n, a;
} SPR;

//static SPR *spr = (SPR *)hram;
#define spr ((SPR *)hram)
// one pointer less :) 

#define SPR_PRI     0x80
#define SPR_FLIPY   0x40
#define SPR_FLIPX   0x20
#define SPR_PAL     0x10

SPR used_spr[40];
/* We must be able to gather 40 sprites first, then sort,then leave 10
since sprites are as big as "int", there is no point in storing indexes
*/

unsigned num_sprites = 0;


//tilecache is 256-color based
// dir can take values: 1-left 2-right
#define DIR_NORMAL 0x20
#define DIR_XMIRROR 0x40


uint8_t tilecachedata[0x2000*4*2];
uint8_t tilecache[512];
unsigned long bitxlat_t[16],bitxlat2_t[16],bitxlatM_t[16],bitxlat2M_t[16];

void tilecache_init(void) {
	unsigned i,tmp;
	memset(tilecache,0,sizeof(tilecache));
	memset(tilecachedata,0,sizeof(tilecachedata));
	for(i=0;i<16;i++) {
		bitxlat_t[i] = tmp = ((i<<24)+(i<<15)+(i<<6)+(i>>3))&0x01010101; // Higher bits first
		bitxlat2_t[i] = tmp << 1;
		bitxlatM_t[i] = tmp = (i+(i<<7)+(i<<14)+(i<<21))&0x01010101;  // lower bits first
		bitxlat2M_t[i] = tmp << 1;
	}
}

static uint8_t* getcell(unsigned celln,unsigned dir) {
	uint8_t*dest=tilecachedata+((dir&0x40)<<9)+(celln<<6),*rp;
	unsigned long *wp;
	register unsigned b0,b1;
	unsigned i;
	if(!(tilecache[celln]&dir)) {
		tilecache[celln]|=dir;
		wp = (unsigned long *)dest;
		rp = vram+(celln<<4);
		
		if(dir&DIR_XMIRROR) 
			for(i=0;i<8;i++) {
				wp[0]=bitxlatM_t[(b0=rp[0])&0xF]|bitxlat2M_t[(b1=rp[1])&0xF];
				wp[1]=bitxlatM_t[b0>>4]|bitxlat2M_t[b1>>4];
				wp+=2;
				rp+=2;
			}
		else
			for(i=0;i<8;i++) {
				wp[0]=bitxlat_t[(b0=rp[0])>>4]|bitxlat2_t[(b1=rp[1])>>4];
				wp[1]=bitxlat_t[b0&0xF]|bitxlat2_t[b1&0xF];
				wp+=2;
				rp+=2;
			}
	}
	return dest;
}


void lcd_enumsprites()
{
	unsigned h = ((R_LCDC & 4)<<1)+8;// ? (16) : (8); sprite height
	unsigned line = (unsigned)(R_LY)+16;
	unsigned i,j,ntosort;
	register unsigned long tmp;
	num_sprites = 0;

	
	if(R_LCDC & 2)
	{

		/* link sprite list */
		for(i=0; i<40; i++)
			if((line-(unsigned)spr[i].y)<h)
				used_spr[num_sprites++] = spr[i];

		if(num_sprites<2) return;
		ntosort=num_sprites-1;
		if(ntosort>10) ntosort = 10;
		for(i=0;i<ntosort;i++) {
			for(j=i+1;j<num_sprites;j++)
				if(used_spr[i].x>used_spr[j].x) {
					tmp=((unsigned long*)used_spr)[i];
					((unsigned long*)used_spr)[i] = ((unsigned long*)used_spr)[j];
					((unsigned long*)used_spr)[j] = tmp;
				}
		}
		if(num_sprites>10) num_sprites = 10;
	}
}

// **********************************************************************


void lcd_refreshline(void) {

	signed char *tilemapptr;
	uint8_t*writeptr,*tmpptr;
	unsigned tileofs,tilepage,tilemapx;
	unsigned X,Y,LY,WX,WY,i,j;
	//unsigned tmp0,tmp1;
	unsigned spriteh = ((R_LCDC & 4)<<1)+7;// sprite height
	unsigned sprtilemask=~((R_LCDC & 4)>>2);
	uint8_t spr_pal,tmp;

	//memset(tilecache,0,sizeof(tilecache));// TODO: SLOW!!! for debug only!!

	benchmark_gfx-=GetTimer();

	LY = R_LY;
	WX=167;

	/*Check for window*/
	if((R_LCDC & 0x20)  /*WIN enable*/
		&& ((WY=(unsigned)R_WY)<=LY)){
		lcd_WYline++;	// TODO: maybe incremented only if R_WX<167?
		if(R_WX<167 /*allowed position*/ )
			WX = R_WX;
	}

	tilepage = ~(((unsigned)R_LCDC&0x10)<<4);
	/*Show background*/
	if((R_LCDC & 1)/*BG enable*/ && (WX>7)/*not covered by WIN*/) {
		X = R_SCX;
		Y = LY + R_SCY;
		writeptr = linebuffer+8-(X&7);
		tilemapptr = (signed char *)(vram+0x1800+(((unsigned)R_LCDC & 8)<<7) // pointer to tile line
					 +((Y&(31*8))<<2));
		tilemapx = (X>>3)&31;
		tileofs = (Y&7)<<3;
		for(i=((X+WX)>>3)-(X>>3);i>0;i--) {
			tmpptr = getcell(
				(((signed int)tilemapptr[tilemapx] +256)&tilepage)
				,DIR_NORMAL)+tileofs;
			((unsigned *)writeptr)[0] = ((unsigned *)tmpptr)[0];
			((unsigned *)writeptr)[1] = ((unsigned *)tmpptr)[1]; // instead of memcpy
			tilemapx=(tilemapx+1)&31;
			writeptr+=8;
		}
	}

	//w_priority=(LCDC&2)
	/*Show window*/
	if(WX<167) {
		//X = 0;
		Y = lcd_WYline; // internal WIN counter used instead of LY-WY;
		writeptr = linebuffer+1+WX;
		tilemapptr = (signed char *)(vram+0x1800+(((unsigned)R_LCDC & 0x40)<<4) // pointer to tile line
					 +((Y&(31*8))<<2));
		tileofs = (Y&7)<<3;

		for(i=((166+8)-WX)>>3;i>0;i--) {
			tmpptr = getcell(
				(((signed int)*(tilemapptr++) +256)&tilepage)
				,DIR_NORMAL)+tileofs;
			((unsigned *)writeptr)[0] = ((unsigned *)tmpptr)[0];
			((unsigned *)writeptr)[1] = ((unsigned *)tmpptr)[1]; // instead of memcpy
			writeptr+=8;
		}

	}
	
	//num_sprites=0;
	for(i=0;i<num_sprites;i++) if((unsigned)((X=used_spr[i].x)-1) < (unsigned)167) {
		X=used_spr[i].x;
		Y=(LY+16-used_spr[i].y);
		if(used_spr[i].a&SPR_FLIPY) Y^=spriteh; // Y flip (bit 6)
		writeptr = linebuffer+X;
		tmpptr=getcell(((unsigned)used_spr[i].n&sprtilemask)+(Y>>3),
			(used_spr[i].a&SPR_FLIPX)+DIR_NORMAL)+((Y&7)<<3);
		spr_pal=((used_spr[i].a&0x10)>>2)+4; 
		if((used_spr[i].a & SPR_PRI)) {  // below everything
			for(j=0;j<8;j++) if(tmpptr[j]) {
				tmp=writeptr[j];
				if(!tmp) tmp = tmpptr[j]+spr_pal;
				writeptr[j]=tmp|0x80; // update pixel priority
			}
		} else {// above background, below previous sprites
			spr_pal|=0x80;
			for(j=0;j<8;j++)
				if(tmpptr[j] && ((signed char)writeptr[j]>=0))
					writeptr[j]=tmpptr[j]+spr_pal;
		}
	}
	lcd_refresh(LY);
	benchmark_gfx+=GetTimer();
}


PLAT void lcd_vsync()
{
	static int first = 1;
	static unsigned long frame = 1;
	char title[64];
	static unsigned oldtime[8][4],timepos=0;
	unsigned time,i,bm_g,bm_o,bm_s;
	lcd_WYline = -1; // reset internal WY line counter
	if(first)
	{
		TimerInit();
		Timer();
		//fps_time = oldtime = GetTickCount();
		first = 0;
	}

	time=GetTimer();
	if(lcd_fpslimit) while((time=GetTimer()) < 1000000/60) ;
	else while ((time = GetTimer()) < 1000000/1000);		// Limited to 1000 FPS
	Timer();
	oldtime[timepos][0]=time;
	oldtime[timepos][1]=benchmark_gfx;
	oldtime[timepos][2]=benchmark_sound;
	benchmark_gfx = benchmark_sound = 0;
	timepos=(timepos+1)&7;

#if (SCALE != 1)

	StretchBlt(
		main_hdc,
		0, 0,
		dib_width * SCALE, -dib_height * SCALE,
		hdcc,
		0, 0,
		dib_width,  -dib_height,
		SRCCOPY);

#else

	BitBlt(
		main_hdc,
		0, 0,
		dib_width, -dib_height,
		hdcc,
		0, 0,
		SRCCOPY);

#endif

	win32_win_update();

	/* Frames Per Second */
	frame++;

	if(!timepos)
	{
		time=bm_o=bm_s=bm_g=0;
		for(i=0;i<8;i++) {
				time+=oldtime[i][0];
				bm_g+=oldtime[i][1];
				bm_s+=oldtime[i][2];
		}
		time/=8;
		bm_g/=8;
		bm_s/=8;
		bm_o=time-bm_s-bm_g;
		//if(time)
		//    sprintf(title, "GameBoy-%15s [%u fps]O:%02dG:%02dS:%02d", cart.title,
		//	1000000/time,(bm_o*100)/time,(bm_g*100)/time,(bm_s*100)/time);
		//else
			sprintf(title, "GameBoy - %s [%u fps]", cart.title, 1000000/time);
		SetWindowText(main_hwnd, title);
	}

	//oldtime = GetTickCount();
}
