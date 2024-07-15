// platform crap (win32) - DIB-sections
#include "pch.h"

int lcd_scale = 4;
int lcd_fpslimit = 1;
int lcd_effect = 1; // possible values: 0,1

unsigned mainpal[64];  // 0-3 BG palette 4-7,8-11 - sprite palettes
uint8_t linebuffer[192]; // showed from 8-th byte

int dib_width, dib_height;

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

unsigned benchmark_sound, benchmark_gfx;

#define WIN_STYLE ( WS_CAPTION | WS_BORDER | WS_SYSMENU )

/*static */HWND main_hwnd;
static HDC main_hdc, hdcc;
static HBITMAP DIB_section;
static HGDIOBJ old_obj;
//static RGBQUAD *pbuf;

RGBQUAD* pbuf;

/* milk to cofee */
RGBQUAD dib_pal[] = {
	{ 143, 231, 255 },   // color #0 (milk)
	{ 95 , 176, 223 },   // color #1
	{ 63 , 120, 144 },   // color #2
	{ 31 , 56 , 79  }    // color #3 (cofee)
};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static int sound_enabled = 1;

	switch (msg)
	{
		case WM_CREATE:
			break;
		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;
		case WM_DESTROY:
			gb_shutdown();
			exit(0);
			break;
		case WM_KEYDOWN:
			switch (wParam)
			{
			case VK_ESCAPE: DestroyWindow(hwnd); break;
			case VK_F12:
				(sound_enabled) ? apu_shutdown() : apu_init(44100);
				sound_enabled ^= 1;
				break;
			case VK_F8:
				lcd_fpslimit ^= 1;
				break;
			case VK_F9:
				lcd_effect ^= 1; // possible values: 0,1
				break;
			}
			break;

		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

void win32_win_init(int width, int height)
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

	if (!RegisterClass(&wc))
		sys_error("couldn't register main window class.");

	rect.left = 0;
	rect.top = 0;
	rect.right = width * lcd_scale;
	rect.bottom = height * lcd_scale;

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

	if (!main_hwnd)
		sys_error("couldn't create main window.");

	ShowWindow(main_hwnd, SW_NORMAL);
	UpdateWindow(main_hwnd);
}

void win32_win_update()
{
	MSG msg;

	if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			gb_shutdown();
			exit(0);
		}
		if (GetMessage(&msg, NULL, 0, 0))
			DispatchMessage(&msg);
	}
}

void WIN_Center(HWND hwnd)
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
	x = w / 2 - win_width / 2;
	y = h / 2 - win_height / 2;
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

void win32_dib_init(int width, int height)
{
	HDC hdc;
	BITMAPINFO* bmi;
	void* DIB_base;

	bmi = (BITMAPINFO*)calloc(sizeof(BITMAPINFO) + 16 * 4, 1);
	main_hdc = hdc = GetDC(main_hwnd);

	//memset(&(bmi->bmiHeader), 0, sizeof(BITMAPINFOHEADER));

	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	dib_width = bmi->bmiHeader.biWidth = width;
	dib_height = bmi->bmiHeader.biHeight = -height;
	bmi->bmiHeader.biPlanes = 1;
	bmi->bmiHeader.biBitCount = 32;
	bmi->bmiHeader.biCompression = BI_RGB;
	//bmi->bmiHeader.

	DIB_section = CreateDIBSection(hdc, bmi, DIB_RGB_COLORS, &DIB_base, NULL, 0);
	pbuf = (RGBQUAD*)DIB_base;

	hdcc = CreateCompatibleDC(hdc);
	if (!hdcc)
		sys_error("CreateCompatibleDC() failed!");

	if (!(old_obj = SelectObject(hdcc, DIB_section)))
		sys_error("SelectObject() failed!");
	free(bmi);
}

void win32_dib_shutdown()
{
	if (hdcc)
	{
		SelectObject(hdcc, old_obj);
		DeleteDC(hdcc);
	}
	if (DIB_section) DeleteObject(DIB_section);
	if (main_hdc) ReleaseDC(main_hwnd, main_hdc);
}

void win32_dib_blit()
{
	if (lcd_scale != 1) {

		StretchBlt(
			main_hdc,
			0, 0,
			dib_width * lcd_scale, -dib_height * lcd_scale,
			hdcc,
			0, 0,
			dib_width, -dib_height,
			SRCCOPY);
	}
	else {

		BitBlt(
			main_hdc,
			0, 0,
			dib_width, -dib_height,
			hdcc,
			0, 0,
			SRCCOPY);

	}
}

void lcd_refresh(int line)
{
	int i;
	unsigned long* p = (unsigned long*)pbuf + 160 * line;
	if (lcd_effect == 1)
		for (i = 0; i < 160; i++)
			p[i] = (0x7F7F7F & (p[i] >> 1)) + (0x7F7F7F & (((unsigned long*)dib_pal)[mainpal[(linebuffer + 8)[i] & 0x3F]] >> 1));
	else
		for (i = 0; i < 160; i++)
			p[i] = ((unsigned long*)dib_pal)[mainpal[(linebuffer + 8)[i] & 0x3F]];
}
