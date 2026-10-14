/* SceEnumWakeUp: UTF-8 to UTF-16 for MTP strings. */
#include "ew_internal.h"

/* UTF-8 to NUL-terminated UTF-16; returns the characters converted */
int mtpUtf8ToUtf16(SceUInt16 *dst, SceSize size, const SceUInt8 *src)
{
	SceUInt16 units[2];
	SceUInt32 c, limit;
	int count = 0, n;

	if (size < 2)
		return 0;
	size -= 2;
	for (;;) {
		c = *src++;
		if (c & 0x80) {
			if ((SceUInt8)(c + 0x3e) > 0x32)	/* lead bytes 0xc2..0xf4 */
				break;
			c &= 0x7f;
			limit = 0x40;
			do {
				c = (*src++ & 0x3f) | (c & (limit - 1)) << 6;
				limit <<= 5;
			} while (c >= limit);
		}
		if (c == 0)
			break;
		if (c < 0x110000 && c - 0xd800 >= 0x800) {
			units[0] = c;
			n = 1;
			if (c >= 0x10000) {
				units[1] = (c & 0x3ff) + 0xdc00;
				units[0] = ((c - 0x10000) >> 10) + 0xd800;
				n = 2;
			}
		} else {
			units[0] = 0;
			n = 1;
		}
		if (size < (SceSize)n * 2)
			break;
		size -= n * 2;
		memcpy(dst, units, n * 2);
		dst += n;
		count++;
	}
	*dst = 0;
	return count;
}
