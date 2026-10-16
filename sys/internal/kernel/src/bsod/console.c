/* BSOD text grid, palette and framebuffer rasterizer. */
#include "bsod_internal.h"
#include <stdarg.h>

#include "font.h"

typedef struct BsodConsole {
	void *pixels;
	unsigned pitch;
	unsigned height;
	unsigned format;
	unsigned glyphWidth;
	unsigned glyphHeight;
	const uint8_t *font;
	unsigned fontStride;
	unsigned columns;
	unsigned rows;
	unsigned x;
	unsigned y;
	unsigned foreground;
	unsigned background;
	int colorCount;
	uint32_t colors[16];
} BsodConsole;
_Static_assert(offsetof(BsodConsole, colors) == 0x3c, "BSOD palette offset");
BsodConsole s_bsodConsole;
uint8_t s_bsodCharacters[160 * 34];
uint8_t s_bsodAttributes[160 * 34];

int __attribute__((noinline)) bsodUpdateGrid(void)
{
	if (s_bsodConsole.glyphWidth)
		s_bsodConsole.columns = s_bsodConsole.pitch / s_bsodConsole.glyphWidth;
	if (s_bsodConsole.fontStride)
		s_bsodConsole.rows = s_bsodConsole.height / s_bsodConsole.fontStride;
	return 0;
}

void bsodSetFramebuffer(void *pixels, unsigned pitch, unsigned height, unsigned format)
{
	s_bsodConsole.pixels = pixels;
	s_bsodConsole.pitch = pitch;
	s_bsodConsole.height = height;
	s_bsodConsole.format = format;
	bsodUpdateGrid();
}

void bsodSetFont(unsigned width, unsigned height)
{
	s_bsodConsole.glyphWidth = width;
	s_bsodConsole.glyphHeight = height;
	s_bsodConsole.font = width == 8 ? s_bsodFont8 : s_bsodFont6;
	s_bsodConsole.fontStride = 16;
	bsodUpdateGrid();
}

static unsigned bsodColorIndex(uint32_t color, unsigned fallback)
{
	int i, count = s_bsodConsole.colorCount;

	for (i = 0; i < count; i++) {
		if (s_bsodConsole.colors[i] == color)
			return i;
	}
	if (count >= 16)
		return fallback;
	s_bsodConsole.colors[count] = color;
	s_bsodConsole.colorCount = count + 1;
	return count;
}

void bsodSetForeground(uint32_t color)
{
	s_bsodConsole.foreground = bsodColorIndex(color, 1);
}

void bsodSetBackground(uint32_t color)
{
	s_bsodConsole.background = bsodColorIndex(color, 0);
}

void bsodResetColors(void)
{
	bsodSetBackground(0);
	bsodSetForeground(0xffffffff);
	s_bsodConsole.x = 0;
	s_bsodConsole.y = 0;
}

void bsodClearGrid(void)
{
	unsigned i;
	uint8_t attribute = s_bsodConsole.foreground | s_bsodConsole.background << 4;

	for (i = 0; i < sizeof(s_bsodCharacters); i++) {
		s_bsodCharacters[i] = 0;
		s_bsodAttributes[i] = attribute;
	}
	s_bsodConsole.x = 0;
	s_bsodConsole.y = 0;
}

int bsodPutCharacter(int character)
{
	unsigned x = s_bsodConsole.x, y = s_bsodConsole.y;
	unsigned columns = s_bsodConsole.columns, rows = s_bsodConsole.rows;
	unsigned i, j, end;
	uint8_t attribute = s_bsodConsole.foreground | s_bsodConsole.background << 4;

	if (character == '\t') {
		end = (x + 8) & ~7U;
		if (columns <= end)
			end = columns;
		while (x < end) {
			s_bsodCharacters[y * 160 + x] = ' ';
			s_bsodAttributes[y * 160 + x++] = attribute;
		}
	} else if (character == '\n') {
		y++;
		x = 0;
	} else {
		s_bsodCharacters[y * 160 + x] = character;
		s_bsodAttributes[y * 160 + x++] = attribute;
	}
	if (columns <= x) {
		y++;
		x = 0;
	}
	if (rows <= y) {
		for (i = 0; i < rows - 1; i++) {
			for (j = 0; j < columns; j++) {
				s_bsodCharacters[i * 160 + j] = s_bsodCharacters[(i + 1) * 160 + j];
				s_bsodAttributes[i * 160 + j] = s_bsodAttributes[(i + 1) * 160 + j];
			}
		}
		y = rows - 1;
		for (j = 0; j < columns; j++) {
			s_bsodCharacters[y * 160 + j] = ' ';
			s_bsodAttributes[y * 160 + j] = attribute;
		}
	}
	s_bsodConsole.x = x;
	s_bsodConsole.y = y;
	return 0;
}

void bsodRasterize(void)
{
	uint16_t palette[16];
	unsigned color, r, g, b, a, i, row, column, gy, gx;
	uint8_t character, attribute, bits;

	if (!s_bsodConsole.pixels)
		return;
	if (s_bsodConsole.format != 3) {
		for (i = 0; i < 16; i++) {
			color = s_bsodConsole.colors[i];
			r = color & 0xff;
			g = color >> 8 & 0xff;
			b = color >> 16 & 0xff;
			a = color >> 24;
			switch (s_bsodConsole.format) {
			case 0:
				palette[i] = r | g << 5 | (b & 0x1f) << 11;
				break;
			case 1:
				palette[i] = r | g << 5 | b << 10 | a << 15;
				break;
			case 2:
				palette[i] = r | g << 4 | b << 8 | a << 12;
				break;
			default:
				palette[i] = 0;
			}
		}
	}
	for (row = 0; row < s_bsodConsole.rows; row++) {
		for (column = 0; column < s_bsodConsole.columns; column++) {
			character = s_bsodCharacters[row * 160 + column];
			attribute = s_bsodAttributes[row * 160 + column];
			for (gy = 0; gy < s_bsodConsole.glyphHeight; gy++) {
				/* any character code indexes the font, not only ASCII */
				bits = s_bsodConsole.font[character * s_bsodConsole.fontStride + gy];
				for (gx = 0; gx < s_bsodConsole.glyphWidth; gx++) {
					i = s_bsodConsole.pitch * (row * s_bsodConsole.glyphHeight + gy) +
					    column * s_bsodConsole.glyphWidth + gx;
					color = (gx & 0xff) < 32 && (bits >> (gx & 0xff) & 1) ? attribute & 0xf : attribute >> 4;
					if (s_bsodConsole.format == 3)
						((uint32_t *)s_bsodConsole.pixels)[i] = s_bsodConsole.colors[color];
					else
						((uint16_t *)s_bsodConsole.pixels)[i] = palette[color];
				}
			}
		}
	}
}

static int bsodFormatCharacter(void *context, const uint32_t *state, int character)
{
	if (character)
		bsodPutCharacter((int8_t)character);
	return 1;
}

void bsodPrintf(const char *format, ...)
{
	uint32_t count = 0;
	va_list args;

	va_start(args, format);
	bsodFormat(bsodFormatCharacter, &count, -1, format, args.__ap);
	va_end(args);
}
