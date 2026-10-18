#include <stdint.h>
#include <libkernel/libkernel.h>

/* The native table folds ASCII capitals; every other byte maps to itself. */
#define IDENTITY16(base) \
	(base) + 0, (base) + 1, (base) + 2, (base) + 3, \
	(base) + 4, (base) + 5, (base) + 6, (base) + 7, \
	(base) + 8, (base) + 9, (base) + 10, (base) + 11, \
	(base) + 12, (base) + 13, (base) + 14, (base) + 15

static const uint16_t asciiLowercase[256] = {
	IDENTITY16(0x00), IDENTITY16(0x10),
	IDENTITY16(0x20), IDENTITY16(0x30),
	'@', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	'x', 'y', 'z', '[', '\\', ']', '^', '_',
	IDENTITY16(0x60), IDENTITY16(0x70),
	IDENTITY16(0x80), IDENTITY16(0x90),
	IDENTITY16(0xa0), IDENTITY16(0xb0),
	IDENTITY16(0xc0), IDENTITY16(0xd0),
	IDENTITY16(0xe0), IDENTITY16(0xf0),
};

#undef IDENTITY16

/* Native lowercase-table pointer; original symbol name unknown. */
const uint16_t *clibLowercaseTable = asciiLowercase;

int sceClibStrncasecmp(const char *left, const char *right, SceSize count)
{
	const unsigned char *first = (const unsigned char *)left;
	const unsigned char *second = (const unsigned char *)right;
	const unsigned char *last;
	const uint16_t *table;
	uint16_t foldedFirst, foldedSecond;

	if (!count)
		return 0;
	table = clibLowercaseTable;
	last = (const unsigned char *)((uintptr_t)first + count - 1);
	for (;;) {
		foldedFirst = table[*first];
		foldedSecond = table[*second];
		if (foldedFirst != foldedSecond)
			return (uint8_t)foldedFirst < (uint8_t)foldedSecond ? -1 : 1;
		if (!*first || first == last)
			return 0;
		first++;
		second++;
	}
}
