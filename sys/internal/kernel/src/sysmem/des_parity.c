/* Shared DES key utilities; names are descriptive (original names unknown). */
#include <sysmem/des.h>
#include <fw.h>

#if FW_SINCE(0x04000000)
/* the debug build counts the bits with NEON, in des.S */
uint64_t DesAdjustKeyParity(uint64_t key);
#else
/* Expand the odd-parity table from its definition instead of opaque bytes. */
#define DES_PARITY_BIT(value) \
	(((value) ^ ((value) >> 1) ^ ((value) >> 2) ^ ((value) >> 3) ^ \
	  ((value) >> 4) ^ ((value) >> 5) ^ ((value) >> 6) ^ ((value) >> 7)) & 1)
#define DES_PARITY(value) ((value) ^ (1 ^ DES_PARITY_BIT(value)))
#define DES_PARITY_ROW(base) \
	DES_PARITY((base) + 0), DES_PARITY((base) + 1), \
	DES_PARITY((base) + 2), DES_PARITY((base) + 3), \
	DES_PARITY((base) + 4), DES_PARITY((base) + 5), \
	DES_PARITY((base) + 6), DES_PARITY((base) + 7), \
	DES_PARITY((base) + 8), DES_PARITY((base) + 9), \
	DES_PARITY((base) + 10), DES_PARITY((base) + 11), \
	DES_PARITY((base) + 12), DES_PARITY((base) + 13), \
	DES_PARITY((base) + 14), DES_PARITY((base) + 15)

static const uint8_t s_oddParity[256] = {
	DES_PARITY_ROW(0x00), DES_PARITY_ROW(0x10), DES_PARITY_ROW(0x20), DES_PARITY_ROW(0x30),
	DES_PARITY_ROW(0x40), DES_PARITY_ROW(0x50), DES_PARITY_ROW(0x60), DES_PARITY_ROW(0x70),
	DES_PARITY_ROW(0x80), DES_PARITY_ROW(0x90), DES_PARITY_ROW(0xa0), DES_PARITY_ROW(0xb0),
	DES_PARITY_ROW(0xc0), DES_PARITY_ROW(0xd0), DES_PARITY_ROW(0xe0), DES_PARITY_ROW(0xf0),
};

uint64_t DesAdjustKeyParity(uint64_t key)
{
	union {
		uint64_t key;
		uint8_t bytes[8];
	} value = { .key = key };
	unsigned int index;

	for (index = 0; index < sizeof(value.bytes); index++)
		value.bytes[index] = s_oddParity[value.bytes[index]];
	return value.key;
}
#endif

int DesCheckKeyParity(uint64_t key)
{
	return DesAdjustKeyParity(key) == key;
}

int DesIsWeakKey(uint64_t key)
{
	unsigned int index;

#if FW_SINCE(0x04000000)
	/* The debug build folds the sixteen DES keys with their complements. */
	static const uint32_t weakKeys[] = {
		0x01010101, 0x1f1f0e0e, 0x01fe01fe, 0x1fe00ef1,
		0x01e001f1, 0x1ffe0efe, 0x1f010e01, 0x011f010e,
	};
	uint32_t low = (uint16_t)key;
	uint32_t high = (uint16_t)(key >> 32);
	uint32_t folded;

	/* a weak key repeats its first and its last two bytes */
	if (key != ((uint64_t)(high * 0x10001u) << 32) + low * 0x10001u)
		return 0;
	folded = low + (high << 16);
	if ((int32_t)folded < (int32_t)~folded)
		folded = ~folded;
	for (index = 0; index < sizeof(weakKeys) / sizeof(weakKeys[0]); index++) {
		if (weakKeys[index] == folded)
			return 1;
	}
#else
	static const uint64_t weakKeys[] = {
		0x0101010101010101ULL, 0xfefefefefefefefeULL,
		0x1f1f1f1f0e0e0e0eULL, 0xe0e0e0e0f1f1f1f1ULL,
		0x01fe01fe01fe01feULL, 0xfe01fe01fe01fe01ULL,
		0x1fe01fe00ef10ef1ULL, 0xe01fe01ff10ef10eULL,
		0x01e001e001f101f1ULL, 0xe001e001f101f101ULL,
		0x1ffe1ffe0efe0efeULL, 0xfe1ffe1ffe0efe0eULL,
		0x011f011f010e010eULL, 0x1f011f010e010e01ULL,
		0xe0fee0fef1fef1feULL, 0xfee0fee0fef1fef1ULL,
	};

	for (index = 0; index < sizeof(weakKeys) / sizeof(weakKeys[0]); index++) {
		if (weakKeys[index] == key)
			return 1;
	}
#endif
	return 0;
}
