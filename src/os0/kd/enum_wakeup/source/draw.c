/* SceEnumWakeUp: Drawing into the frame buffer. */
#include "ew_internal.h"

int ewDrawSetTarget(SceUInt32 *base, int width, int height, int pitch)
{
	ewg.draw.base = base;
	ewg.draw.width = width;
	ewg.draw.height = height;
	ewg.draw.pitch = pitch;
	return 0;
}

int ewDrawClear(SceUInt32 color)
{
	SceUInt32 *line = ewg.draw.base, *p;
	int x, y;

	/* four pixels at a time: a width not divisible by 4 is rounded up */
	for (y = 0; y < ewg.draw.height; y++, line += ewg.draw.width)
		for (x = 0, p = line; x < ewg.draw.width; x += 4, p += 4) {
			p[0] = color;
			p[1] = color;
			p[2] = color;
			p[3] = color;
		}
	return 0;
}

int ewDrawPixel(SceUInt32 color, int x, int y)
{
	ewg.draw.base[ewg.draw.width * y + x] = color;
	return 0;
}
