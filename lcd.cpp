// Displaying the picture on the LCD
#include "pch.h"

int lcd_scale = 4;
int lcd_fpslimit = 1;
int lcd_effect = 1; // possible values: 0,1

unsigned mainpal[64];  // 0-3 BG palette 4-7,8-11 - sprite palettes

int screen_width, screen_height;
static RGBQUAD* pbuf;

/* milk to cofee */
RGBQUAD dib_pal[] = {
	{ 143, 231, 255 },   // color #0 (milk)
	{ 95 , 176, 223 },   // color #1
	{ 63 , 120, 144 },   // color #2
	{ 31 , 56 , 79  }    // color #3 (cofee)
};

SDL_Surface* output_surface = nullptr;
SDL_Window* output_window = nullptr;

//case VK_F12:
//	(sound_enabled) ? apu_shutdown() : apu_init(44100);
//	sound_enabled ^= 1;
//	break;
//case VK_F8:
//	lcd_fpslimit ^= 1;
//	break;
//case VK_F9:
//	lcd_effect ^= 1; // possible values: 0,1
//	break;

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

void sdl_win_init(int width, int height)
{
	screen_width = width;
	screen_height = height;

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		__log ("SDL video could not initialize! SDL_Error: %s\n", SDL_GetError());
		return;
	}

	char title[128];
	sprintf(title, "GameBoy - %s", romhdr->title);

	SDL_Window* window = SDL_CreateWindow(
		title,
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_width * lcd_scale, screen_height * lcd_scale,
		0);

	if (window == NULL) {
		__log ("SDL_CreateWindow failed: %s\n", SDL_GetError());
		return;
	}

	SDL_Surface* surface = SDL_GetWindowSurface(window);

	if (surface == NULL) {
		__log ("SDL_GetWindowSurface failed: %s\n", SDL_GetError());
		return;
	}

	// Initialize window to all black
	//SDL_FillSurfaceRect(surface, NULL, SDL_MapRGB(surface->format, 0, 0, 0));
	SDL_UpdateWindowSurface(window);

	output_window = window;
	output_surface = surface;

	pbuf = new RGBQUAD[screen_width * screen_height];
	memset(pbuf, 0, screen_width * screen_height * sizeof(RGBQUAD));
}

void sdl_win_shutdown()
{
	SDL_DestroyWindow(output_window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	delete[] pbuf;
}

void sdl_win_update()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT) {
			gb_shutdown();
			exit(0);
		}
	}
}

void sdl_win_blit()
{
	int w = screen_width;
	int h = screen_height;
	int ScaleFactor = lcd_scale;

	Uint32* const pixels = (Uint32*)output_surface->pixels;

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			RGBQUAD color = pbuf[w * y + x];

			for (int s = 0; s < ScaleFactor; s++) {
				for (int t = 0; t < ScaleFactor; t++) {
					pixels[ScaleFactor * x + s + ((ScaleFactor * y + t) * output_surface->w)] = *(uint32_t *)&color;
				}
			}
		}
	}

	SDL_UpdateWindowSurface(output_window);
}

void sdl_win_update_title(char* title)
{
	if (!output_window)
		return;
	SDL_SetWindowTitle(output_window, title);
}
