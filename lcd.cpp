// Displaying the picture on the LCD
#include "pch.h"

int lcd_scale = 4;
int lcd_fpslimit = 1;
int lcd_effect = 1; // possible values: 0,1
int lcd_border = 0;

int screen_width, screen_height;
static uint32_t* pbuf;

/* milk to cofee */
uint32_t dmg_pal[] = {
	0xffe78f,		// color #0 (milk)
	0xdfb05f,		// color #1
	0x90783f,		// color #2
	0x4f381f,		// color #3 (cofee)
};

SDL_Surface* output_surface = nullptr;
SDL_Window* output_window = nullptr;

void lcd_refresh(int line)
{
	int i;
	uint32_t* p = (uint32_t*)pbuf + 160 * line;
	if (lcd_effect == 1)
		for (i = 0; i < 160; i++)
			p[i] = (0x7F7F7F & (p[i] >> 1)) + (0x7F7F7F & (((uint32_t*)dmg_pal)[mainpal[(linebuffer + 8)[i] & 0x3F]] >> 1));
	else
		for (i = 0; i < 160; i++)
			p[i] = ((uint32_t*)dmg_pal)[mainpal[(linebuffer + 8)[i] & 0x3F]];
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
	sprintf(title, "GameBoy - %s", cart.title);

	SDL_Window* window = SDL_CreateWindow(
		title,
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_width * lcd_scale + 2 * lcd_border * lcd_scale, screen_height * lcd_scale + 2 * lcd_border * lcd_scale,
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

	pbuf = new uint32_t[screen_width * screen_height];
	memset(pbuf, 0, screen_width * screen_height * sizeof(uint32_t));
}

void sdl_win_shutdown()
{
	SDL_DestroyWindow(output_window);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
	delete[] pbuf;
}

/* The picture buffer, for the save state (see lcd.h). lcd_refresh() keeps
   blending into this buffer, so it is state just as much as VRAM is. */
uint32_t* lcd_screen_buffer(void)
{
	return pbuf;
}

int lcd_screen_pixels(void)
{
	return pbuf ? screen_width * screen_height : 0;
}

/* =================== bottom-of-screen overlay ===================
   A small message box shown over the lower part of the picture when a save
   state is written/read or the slot is switched (see F5/F6/F7 in
   sdl_win_update).  The box is only as wide as its message, so it covers
   very little of the game; the picture keeps showing through it, dimmed.
   It is drawn straight into the window surface right after the picture has
   been copied there, so the emulator framebuffer (pbuf) is never touched
   and the box goes away by itself: the next blit after lcd_overlay_show()'s
   time is up simply does not draw it again.                            */

#define OVERLAY_TIME_MS  1500
#define OVERLAY_FONT_W   3
#define OVERLAY_FONT_H   5
#define OVERLAY_GAP      1      /* empty columns between two characters */
#define OVERLAY_PAD_X    2      /* box padding around the text ... */
#define OVERLAY_PAD_Y    2
#define OVERLAY_MARGIN   2      /* ... and the distance from the bottom edge */

/* 3x5 font, one byte per row, bit 2 is the leftmost of the three columns.
   Only upper case, digits and a few punctuation marks are needed, but the
   whole set is cheap and keeps the messages readable. */
static const uint8_t overlay_font[][OVERLAY_FONT_H] = {
	{ 0,0,0,0,0 },			/* ' ' */
	{ 2,5,7,5,5 },			/* A */
	{ 6,5,6,5,6 },			/* B */
	{ 3,4,4,4,3 },			/* C */
	{ 6,5,5,5,6 },			/* D */
	{ 7,4,6,4,7 },			/* E */
	{ 7,4,6,4,4 },			/* F */
	{ 3,4,5,5,3 },			/* G */
	{ 5,5,7,5,5 },			/* H */
	{ 7,2,2,2,7 },			/* I */
	{ 1,1,1,5,2 },			/* J */
	{ 5,5,6,5,5 },			/* K */
	{ 4,4,4,4,7 },			/* L */
	{ 5,7,7,5,5 },			/* M */
	{ 5,6,5,5,5 },			/* N */
	{ 2,5,5,5,2 },			/* O */
	{ 6,5,6,4,4 },			/* P */
	{ 2,5,5,6,3 },			/* Q */
	{ 6,5,6,5,5 },			/* R */
	{ 3,4,2,1,6 },			/* S */
	{ 7,2,2,2,2 },			/* T */
	{ 5,5,5,5,2 },			/* U */
	{ 5,5,5,2,2 },			/* V */
	{ 5,5,7,7,5 },			/* W */
	{ 5,5,2,5,5 },			/* X */
	{ 5,5,2,2,2 },			/* Y */
	{ 7,1,2,4,7 },			/* Z */
	{ 2,5,5,5,2 },			/* 0 */
	{ 2,6,2,2,7 },			/* 1 */
	{ 6,1,2,4,7 },			/* 2 */
	{ 7,1,3,1,7 },			/* 3 */
	{ 5,5,7,1,1 },			/* 4 */
	{ 7,4,6,1,6 },			/* 5 */
	{ 3,4,7,5,3 },			/* 6 */
	{ 7,1,2,2,2 },			/* 7 */
	{ 2,5,2,5,2 },			/* 8 */
	{ 7,5,7,1,6 },			/* 9 */
	{ 0,0,7,0,0 },			/* - */
	{ 0,0,0,0,2 },			/* . */
	{ 0,2,0,2,0 },			/* : */
	{ 2,2,2,0,2 },			/* ! */
	{ 6,1,2,0,2 },			/* ? */
};

/* the glyphs above, in the order they were written */
static const char overlay_font_map[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-.:!?";

static_assert(sizeof(overlay_font) / OVERLAY_FONT_H == sizeof(overlay_font_map) - 1,
	"the overlay font and its character map are out of sync");

static char     overlay_text[32];
static uint32_t overlay_until_ms;
static int      overlay_active;

/* Show a message in the overlay for OVERLAY_TIME_MS milliseconds */
void lcd_overlay_show(const char* text)
{
	int i;

	if (!text) text = "";
	for (i = 0; i < (int)sizeof(overlay_text) - 1 && text[i]; i++)
		overlay_text[i] = text[i];
	overlay_text[i] = '\0';

	overlay_until_ms = SDL_GetTicks() + OVERLAY_TIME_MS;
	overlay_active = 1;
}

static const uint8_t* overlay_glyph(char c)
{
	unsigned i;

	if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
	for (i = 0; i < sizeof(overlay_font_map) - 1; i++)
		if (overlay_font_map[i] == c)
			return overlay_font[i];

	return overlay_font[0];	/* anything else shows up as a blank */
}

/* Fill a rectangle given in GameBoy screen coordinates; the window surface
   is lcd_scale times larger than the picture, which is done here. */
static void overlay_fill(int x, int y, int w, int h, uint32_t color)
{
	Uint32* pixels;
	int px, py, s, t;

	if (!output_surface) return;
	pixels = (Uint32*)output_surface->pixels;

	for (py = y; py < y + h; py++) {
		if (py < 0 || py >= screen_height) continue;
		for (px = x; px < x + w; px++) {
			if (px < 0 || px >= screen_width) continue;
			for (s = 0; s < lcd_scale; s++)
				for (t = 0; t < lcd_scale; t++)
					pixels[lcd_scale * (px + lcd_border) + s +
						(lcd_scale * (py + lcd_border) + t) * output_surface->w] = color;
		}
	}
}

static void overlay_draw(void)
{
	const uint8_t* g;
	Uint32* pixels;
	int i, n, cellw, textw, boxw, boxh, x0, y0, x, y, gx, gy, cx, cy;

	if (!overlay_active) return;
	if (!output_surface) { overlay_active = 0; return; }
	if ((int32_t)(SDL_GetTicks() - overlay_until_ms) >= 0) { overlay_active = 0; return; }

	/* a box just big enough for the message, centred at the bottom */
	n = (int)strlen(overlay_text);
	cellw = OVERLAY_FONT_W + OVERLAY_GAP;
	textw = n ? n * cellw - OVERLAY_GAP : 0;
	boxw = textw + 2 * OVERLAY_PAD_X;
	boxh = OVERLAY_FONT_H + 2 * OVERLAY_PAD_Y;
	if (boxw > screen_width) boxw = screen_width;
	if (boxh > screen_height) boxh = screen_height;
	x0 = (screen_width - boxw) / 2;
	y0 = screen_height - boxh - OVERLAY_MARGIN;
	if (y0 < 0) y0 = 0;

	/* dim what is underneath instead of covering it with a solid colour */
	pixels = (Uint32*)output_surface->pixels;
	for (y = y0; y < y0 + boxh; y++) {
		for (x = x0; x < x0 + boxw; x++) {
			uint32_t color = 0x7F7F7F & (pbuf[screen_width * y + x] >> 1);
			int s, t;
			for (s = 0; s < lcd_scale; s++)
				for (t = 0; t < lcd_scale; t++)
					pixels[lcd_scale * (x + lcd_border) + s +
						(lcd_scale * (y + lcd_border) + t) * output_surface->w] = color;
		}
	}

	/* and the message on top of it */
	gx = x0 + OVERLAY_PAD_X;
	gy = y0 + OVERLAY_PAD_Y;
	for (i = 0; i < n; i++) {
		g = overlay_glyph(overlay_text[i]);
		for (cy = 0; cy < OVERLAY_FONT_H; cy++)
			for (cx = 0; cx < OVERLAY_FONT_W; cx++)
				if ((g[cy] & (4 >> cx)) &&			/* stay inside */
					(gx + i * cellw + cx) < (x0 + boxw) &&
					(gy + cy) < (y0 + boxh))
					overlay_fill(gx + i * cellw + cx, gy + cy, 1, 1, dmg_pal[0]);
	}
}

/* =================== TEMPORARY per-frame state snapshot =================== */
/* Debug hook - only compiled with DMGEMU_DEBUG_HOOKS (see dbghooks.h). */

void sdl_win_update()
{
	dbg_snap_frame(pbuf, screen_width, screen_height);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				gb_shutdown();
				exit(0);
				break;

			case SDL_KEYDOWN:
			case SDL_KEYUP:
				bool pressed = event.type == SDL_KEYDOWN;
				switch (event.key.keysym.scancode) {
					case SDL_SCANCODE_F5:	/* save state to the current slot */
						if (!pressed && !event.key.repeat) {
							char msg[32];
							if (savestate_save(savestate_slot()))
								sprintf(msg, "SAVED SLOT %d", savestate_slot());
							else
								sprintf(msg, "SAVE FAILED");
							lcd_overlay_show(msg);
						}
						break;
					case SDL_SCANCODE_F6:	/* select the next slot */
						if (!pressed && !event.key.repeat) {
							char msg[32];
							savestate_next_slot();
							sprintf(msg, "SLOT %d", savestate_slot());
							lcd_overlay_show(msg);
						}
						break;
					case SDL_SCANCODE_F7:	/* load state from the current slot */
						if (!pressed && !event.key.repeat) {
							char msg[32];
							switch (savestate_load(savestate_slot())) {
								case SS_LOAD_OK:
									sprintf(msg, "LOADED SLOT %d", savestate_slot());
									break;
								case SS_LOAD_EMPTY:
									sprintf(msg, "EMPTY SLOT %d", savestate_slot());
									break;
								case SS_LOAD_OTHER_ROM:
									sprintf(msg, "WRONG ROM");
									break;
								default:
									sprintf(msg, "BAD SLOT %d", savestate_slot());
									break;
							}
							lcd_overlay_show(msg);
						}
						break;
					case SDL_SCANCODE_F12:
						if (!pressed) {
							(sound_enabled) ? apu_shutdown() : apu_init(44100);
							sound_enabled ^= 1;
						}
						break;
					case SDL_SCANCODE_F8:
						if (!pressed) {
							lcd_fpslimit ^= 1;
						}
						break;
					case SDL_SCANCODE_F9:
						if (!pressed) {
							lcd_effect ^= 1;
						}
						break;

					default:
						pad_sdl_process(event.key.keysym.scancode, pressed);
						break;
				}
				break;
		}
	}
}

void sdl_win_blit()
{
	int w = screen_width;
	int h = screen_height;
	int ScaleFactor = lcd_scale;

	Uint32* const pixels = (Uint32*)output_surface->pixels;

	if (lcd_border != 0) {
		for (int n = 0; n < output_surface->w * output_surface->h; n++) {
			pixels[n] = dmg_pal[0];
		}
	}

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			uint32_t color = pbuf[w * y + x];

			for (int s = 0; s < ScaleFactor; s++) {
				for (int t = 0; t < ScaleFactor; t++) {
					pixels[ScaleFactor * (x + lcd_border) + s + ((ScaleFactor * (y + lcd_border) + t) * output_surface->w)] = color;
				}
			}
		}
	}

	overlay_draw();

	SDL_UpdateWindowSurface(output_window);
}

void sdl_win_update_title(char* title)
{
	if (!output_window)
		return;
	SDL_SetWindowTitle(output_window, title);
}
