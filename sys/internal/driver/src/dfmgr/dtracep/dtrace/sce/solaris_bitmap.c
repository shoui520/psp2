/*
 * Solaris bitmap helpers (Sony's dtrace/sce/solaris_bitmap.c).
 */
#include <dtrace/sce/solaris_bitmap.h>

/* the 1-based index of the highest set bit, 0 for none */
int highbit(ulong_t i)
{
	int h = 1;

	if (i == 0)
		return 0;
	if (i & 0xffff0000) {
		h += 16;
		i >>= 16;
	}
	if (i & 0xff00) {
		h += 8;
		i >>= 8;
	}
	if (i & 0xf0) {
		h += 4;
		i >>= 4;
	}
	if (i & 0xc) {
		h += 2;
		i >>= 2;
	}
	if (i & 0x2)
		h += 1;
	return h;
}
