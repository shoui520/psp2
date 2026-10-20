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

static inline __attribute__((always_inline)) unsigned bsodColorIndex(uint32_t color, unsigned fallback)
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
	unsigned row, column;
	uint8_t attribute = s_bsodConsole.foreground | s_bsodConsole.background << 4;

	for (row = 0; row < sizeof(s_bsodCharacters); row += 160) {
		for (column = 0; column < 160; column++) {
			s_bsodCharacters[row + column] = 0;
			s_bsodAttributes[row + column] = attribute;
		}
	}
	s_bsodConsole.x = 0;
	s_bsodConsole.y = 0;
}

int bsodPutCharacter(int character)
{
	unsigned x = s_bsodConsole.x, y = s_bsodConsole.y;
	unsigned columns, rows, i, j, end, line;
	uint8_t attribute = s_bsodConsole.foreground | s_bsodConsole.background << 4;

	if (character == '\t') {
		end = (x + 8) & ~7U;
		if (end >= s_bsodConsole.columns)
			end = s_bsodConsole.columns;
		while (x < end) {
			s_bsodAttributes[y * 160 + x] = attribute;
			s_bsodCharacters[y * 160 + x++] = ' ';
		}
	} else if (character == '\n') {
		y++;
		x = 0;
	} else {
		s_bsodCharacters[y * 160 + x] = character;
		s_bsodAttributes[y * 160 + x++] = attribute;
	}
	columns = s_bsodConsole.columns;
	rows = s_bsodConsole.rows;
	if (x >= columns) {
		y++;
		x = 0;
	}
	if (y >= rows) {
		for (i = 0, line = 0; i < rows - 1; i++, line += 160) {
			for (j = 0; j != columns; j++) {
				s_bsodCharacters[line + j] = s_bsodCharacters[line + 160 + j];
				s_bsodAttributes[line + j] = s_bsodAttributes[line + 160 + j];
			}
		}
		y = rows - 1;
		for (j = 0; j != columns; j++) {
			s_bsodAttributes[y * 160 + j] = attribute;
			s_bsodCharacters[y * 160 + j] = ' ';
		}
	}
	s_bsodConsole.x = x;
	s_bsodConsole.y = y;
	return 0;
}

int bsodRasterize(void)
{
	uint16_t palette[16];
	void *pixels;
	const uint8_t *font, *glyph;
	unsigned format, color, i, row, column, line, top, left, bit;
	uint8_t attribute, bits;

	if (s_bsodConsole.pixels) {
		format = s_bsodConsole.format;
		if (format != 3) {
			for (i = 0; i < 16; i++) {
				color = s_bsodConsole.colors[i];
				switch (format) {
				case 0:
					palette[i] = (color & 0xff) | (color >> 8 & 0xff) << 5 |
						     (color >> 16 & 0xff) << 11;
					break;
				case 1:
					palette[i] = (color & 0xff) | (color >> 8 & 0xff) << 5 |
						     (color >> 16 & 0xff) << 10 | (color >> 24) << 15;
					break;
				case 2:
					palette[i] = (color & 0xff) | (color >> 8 & 0xff) << 4 |
						     (color >> 16 & 0xff) << 8 | (color >> 24) << 12;
					break;
				default:
					palette[i] = 0;
					break;
				}
			}
		}
		font = s_bsodConsole.font;
		pixels = s_bsodConsole.pixels;
		top = 0;
		for (row = 0; row < s_bsodConsole.rows; row++) {
			left = 0;
			for (column = 0; column < s_bsodConsole.columns; column++) {
				attribute = s_bsodAttributes[row * 160 + column];
				/* any character code indexes the font, not only ASCII */
				glyph = font + s_bsodConsole.fontStride * s_bsodCharacters[row * 160 + column];
				for (line = top; line - top < s_bsodConsole.glyphHeight; line++) {
					bits = glyph[line - top];
					if (s_bsodConsole.format == 3) {
						uint32_t foreground = s_bsodConsole.colors[attribute & 0xf];
						uint32_t background = s_bsodConsole.colors[attribute >> 4];
						uint32_t *pixel = (uint32_t *)pixels +
							left + s_bsodConsole.pitch * line;

						for (bit = 0; bit < s_bsodConsole.glyphWidth; bit++)
							pixel[bit] = bits >> bit & 1 ? foreground : background;
					} else {
						uint16_t foreground = palette[attribute & 0xf];
						uint16_t background = palette[attribute >> 4];
						uint16_t *pixel = (uint16_t *)pixels +
							left + s_bsodConsole.pitch * line;

						for (bit = 0; bit != s_bsodConsole.glyphWidth; bit++)
							pixel[bit] = bits >> bit & 1 ? foreground : background;
					}
				}
				left += s_bsodConsole.glyphWidth;
			}
			top += s_bsodConsole.glyphHeight;
		}
	}
	return 0;
}

static int bsodFormatCharacter(void *context, const uint32_t *state, int character)
{
	if (character)
		bsodPutCharacter((int8_t)character);
	return 1;
}

void bsodPrintf(const char *format, ...)
{
	va_list args;
	uint32_t count = 0;

	va_start(args, format);
	bsodFormat(bsodFormatCharacter, &count, -1, format, args.__ap);
	va_end(args);
}
