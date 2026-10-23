/*
 * SceAppMgr's debug console: a screen of text cells, each a character and a colour attribute
 * (foreground in the low nibble, background in the high one, indices into a palette of up to
 * 16 colours), drawn into a framebuffer with an 8x16 font. The abort handler uses it to print
 * onto the core dump texture.
 */
#include <scetypes.h>
#include <string.h>
#include "debug_console.h"
#include "debug_console_font.h"

#define DEBUG_CONSOLE_COLUMNS	160
#define DEBUG_CONSOLE_ROWS	34

typedef struct AppmgrDebugConsole {
	void *buffer;			/* the framebuffer drawn into, NULL until one is set */
	SceUInt32 width;		/* its size in pixels */
	SceUInt32 height;
	int format;			/* DEBUG_CONSOLE_FORMAT_* */
	SceUInt32 glyphWidth;		/* a cell in pixels */
	SceUInt32 glyphHeight;
	const unsigned char *font;
	SceUInt32 glyphSize;		/* bytes per glyph, one per pixel row */
	SceUInt32 columns;		/* the cells that fit the framebuffer */
	SceUInt32 rows;
	SceUInt32 cursorX;
	SceUInt32 cursorY;
	int foreColor;			/* palette indices */
	int backColor;
	int paletteCount;
	SceUInt32 palette[16];		/* 0xAABBGGRR */
} AppmgrDebugConsole;

_Static_assert(sizeof(AppmgrDebugConsole) == 0x7c, "debug console");

static unsigned char g_appmgrDebugConsoleText[DEBUG_CONSOLE_ROWS][DEBUG_CONSOLE_COLUMNS];
static unsigned char g_appmgrDebugConsoleAttributes[DEBUG_CONSOLE_ROWS][DEBUG_CONSOLE_COLUMNS];
static AppmgrDebugConsole g_appmgrDebugConsole;

int appmgrDebugConsoleInitialize(void)
{
	int row;

	g_appmgrDebugConsole.font = &g_appmgrDebugConsoleFont[0][0];
	g_appmgrDebugConsole.glyphWidth = 8;
	g_appmgrDebugConsole.glyphHeight = 16;
	g_appmgrDebugConsole.glyphSize = 16;
	g_appmgrDebugConsole.columns = g_appmgrDebugConsole.width / 8;
	g_appmgrDebugConsole.rows = g_appmgrDebugConsole.height / 16;
	g_appmgrDebugConsole.buffer = NULL;
	memset(g_appmgrDebugConsole.palette, 0, sizeof(g_appmgrDebugConsole.palette));
	g_appmgrDebugConsole.backColor = 0;
	g_appmgrDebugConsole.cursorX = 0;
	g_appmgrDebugConsole.cursorY = 0;
	g_appmgrDebugConsole.palette[1] = 0xffffffff;
	g_appmgrDebugConsole.paletteCount = 2;
	g_appmgrDebugConsole.foreColor = 1;
	for (row = 0; row < DEBUG_CONSOLE_ROWS; row++) {
		memset(g_appmgrDebugConsoleText[row], 0, DEBUG_CONSOLE_COLUMNS);
		memset(g_appmgrDebugConsoleAttributes[row], 1, DEBUG_CONSOLE_COLUMNS);
	}
	return 0;
}

int appmgrDebugConsoleSetBuffer(void *buffer, SceUInt32 width, SceUInt32 height, int format)
{
	g_appmgrDebugConsole.buffer = buffer;
	g_appmgrDebugConsole.width = width;
	g_appmgrDebugConsole.height = height;
	g_appmgrDebugConsole.format = format;
	if (g_appmgrDebugConsole.glyphWidth != 0)
		g_appmgrDebugConsole.columns = width / g_appmgrDebugConsole.glyphWidth;
	if (g_appmgrDebugConsole.glyphSize != 0)
		g_appmgrDebugConsole.rows = height / g_appmgrDebugConsole.glyphSize;
	return 0;
}

/* The palette index of color, added when the palette has room; else the default index. */
#define DEBUG_CONSOLE_SET_COLOR(field, color, fallback) \
	do { \
		int index; \
		for (index = 0; index < g_appmgrDebugConsole.paletteCount && index < 16; index++) { \
			if (g_appmgrDebugConsole.palette[index] == (color)) { \
				g_appmgrDebugConsole.field = index; \
				return; \
			} \
		} \
		if (g_appmgrDebugConsole.paletteCount < 16) { \
			index = g_appmgrDebugConsole.paletteCount; \
			g_appmgrDebugConsole.field = index; \
			g_appmgrDebugConsole.palette[index] = (color); \
			g_appmgrDebugConsole.paletteCount = index + 1; \
			return; \
		} \
		g_appmgrDebugConsole.field = (fallback); \
	} while (0)

void appmgrDebugConsoleSetForeColor(SceUInt32 color)
{
	DEBUG_CONSOLE_SET_COLOR(foreColor, color, 1);
}

void appmgrDebugConsoleSetBackColor(SceUInt32 color)
{
	DEBUG_CONSOLE_SET_COLOR(backColor, color, 0);
}

void appmgrDebugConsoleSetCursor(SceUInt32 x, SceUInt32 y)
{
	g_appmgrDebugConsole.cursorX = x;
	g_appmgrDebugConsole.cursorY = y;
}

int appmgrDebugConsoleClear(void)
{
	int attribute, row;

	attribute = (unsigned char)(g_appmgrDebugConsole.foreColor |
				    g_appmgrDebugConsole.backColor << 4);
	for (row = 0; row < DEBUG_CONSOLE_ROWS; row++) {
		memset(g_appmgrDebugConsoleText[row], 0, DEBUG_CONSOLE_COLUMNS);
		memset(g_appmgrDebugConsoleAttributes[row], attribute, DEBUG_CONSOLE_COLUMNS);
	}
	g_appmgrDebugConsole.cursorX = 0;
	g_appmgrDebugConsole.cursorY = 0;
	return 0;
}

int appmgrDebugConsolePutChar(SceChar8 c)
{
	SceUInt32 x, y, next, columns, row, column;
	int attribute;

	x = g_appmgrDebugConsole.cursorX;
	y = g_appmgrDebugConsole.cursorY;
	attribute = (unsigned char)(g_appmgrDebugConsole.foreColor |
				    g_appmgrDebugConsole.backColor << 4);
	columns = g_appmgrDebugConsole.columns;
	if (c == '\t') {
		next = (x + 8) & ~7u;
		if (next >= columns)
			next = columns;
		for (; x < next; x++) {
			g_appmgrDebugConsoleText[y][x] = ' ';
			g_appmgrDebugConsoleAttributes[y][x] = attribute;
		}
	} else if (c == '\n') {
		x = 0;
		y++;
	} else {
		g_appmgrDebugConsoleText[y][x] = c;
		g_appmgrDebugConsoleAttributes[y][x] = attribute;
		x++;
	}
	if (x >= columns) {
		x = 0;
		y++;
	}
	if (y >= g_appmgrDebugConsole.rows) {
		/* scroll up a line and clear the last */
		y = g_appmgrDebugConsole.rows - 1;
		for (row = 1; row <= y; row++) {
			for (column = 0; column < columns; column++) {
				g_appmgrDebugConsoleText[row - 1][column] =
					g_appmgrDebugConsoleText[row][column];
				g_appmgrDebugConsoleAttributes[row - 1][column] =
					g_appmgrDebugConsoleAttributes[row][column];
			}
		}
		for (column = 0; column < columns; column++) {
			g_appmgrDebugConsoleText[y][column] = ' ';
			g_appmgrDebugConsoleAttributes[y][column] = attribute;
		}
	}
	g_appmgrDebugConsole.cursorX = x;
	g_appmgrDebugConsole.cursorY = y;
	return 0;
}

int appmgrDebugConsolePuts(const SceChar8 *string)
{
	if (string == NULL)
		return 0;
	while (*string != '\0')
		appmgrDebugConsolePutChar(*string++);
	return 0;
}

int appmgrDebugConsoleFlush(void)
{
	unsigned short colors[16];
	unsigned char *buffer;
	const unsigned char *glyph;
	SceUInt32 color, row, column, line, pixel, left, top, bits;
	int format, index, attribute;

	buffer = g_appmgrDebugConsole.buffer;
	if (buffer == NULL)
		return 0;
	format = g_appmgrDebugConsole.format;
	if (format != DEBUG_CONSOLE_FORMAT_RGBA8888) {
		for (index = 0; index < 16; index++) {
			color = g_appmgrDebugConsole.palette[index];
			if (format == DEBUG_CONSOLE_FORMAT_RGB565)
				colors[index] = (color & 0xff) | (color >> 8 & 0xff) << 5 |
						(color >> 16 & 0xff) << 11;
			else if (format == DEBUG_CONSOLE_FORMAT_RGBA5551)
				colors[index] = (color & 0xff) | (color >> 8 & 0xff) << 5 |
						(color >> 16 & 0xff) << 10 | (color >> 24) << 15;
			else if (format == DEBUG_CONSOLE_FORMAT_RGBA4444)
				colors[index] = (color & 0xff) | (color >> 8 & 0xff) << 4 |
						(color >> 16 & 0xff) << 8 | (color >> 24) << 12;
			else
				colors[index] = 0;
		}
	}
	top = 0;
	for (row = 0; row < g_appmgrDebugConsole.rows; row++) {
		left = 0;
		for (column = 0; column < g_appmgrDebugConsole.columns; column++) {
			attribute = g_appmgrDebugConsoleAttributes[row][column];
			glyph = g_appmgrDebugConsole.font +
				g_appmgrDebugConsole.glyphSize * g_appmgrDebugConsoleText[row][column];
			for (line = 0; line < g_appmgrDebugConsole.glyphHeight; line++) {
				bits = glyph[line];
				if (g_appmgrDebugConsole.format != DEBUG_CONSOLE_FORMAT_RGBA8888) {
					unsigned short foreground = colors[attribute & 0xf];
					unsigned short background = colors[attribute >> 4];
					unsigned short *pixels = (unsigned short *)buffer + left +
						g_appmgrDebugConsole.width * (top + line);

					for (pixel = 0; pixel < g_appmgrDebugConsole.glyphWidth; pixel++)
						pixels[pixel] = (bits >> pixel & 1) ? foreground : background;
				} else {
					SceUInt32 foreground = g_appmgrDebugConsole.palette[attribute & 0xf];
					SceUInt32 background = g_appmgrDebugConsole.palette[attribute >> 4];
					SceUInt32 *pixels = (SceUInt32 *)buffer + left +
						g_appmgrDebugConsole.width * (top + line);

					for (pixel = 0; pixel < g_appmgrDebugConsole.glyphWidth; pixel++)
						pixels[pixel] = (bits >> pixel & 1) ? foreground : background;
				}
			}
			left += g_appmgrDebugConsole.glyphWidth;
		}
		top += g_appmgrDebugConsole.glyphHeight;
	}
	return 0;
}
