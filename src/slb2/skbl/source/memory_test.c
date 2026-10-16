/*
 * SKBL: the boot-time memory test (memory_test.c in Sony's debug records).
 * KblSimpleMemoryTest writes a pseudo-random word every step bytes of the
 * range, then reads each back with the access width the value's bits 8-10
 * choose (a byte, a half-word or the word), and stops on the first
 * mismatch. The debug firmware prints the seed and each block's first
 * address and value as it goes.
 */

#include <stdint.h>
#include <psp2kern/types.h>
#include <sce/debug.h>

/* the next test value: an LCG whose increment is its multiplier */
#define MEMORY_TEST_FACTOR	0x91e6d6a5
#define MEMORY_TEST_NEXT(_v)	((_v) * MEMORY_TEST_FACTOR + MEMORY_TEST_FACTOR)

void KblSimpleMemoryTest(uintptr_t start, SceSize size, uint32_t seed, SceSize step,
			 SceSize blockSize)
{
	uint8_t *end = (uint8_t *)start + size;
	uint8_t *addr, *block;
	uint32_t value, word;
	uint16_t half;
	uint8_t byte;
	unsigned int i;

	value = seed;
	for (addr = (uint8_t *)start; addr < end; addr += step) {
		value = MEMORY_TEST_NEXT(value);
		*(uint32_t *)addr = value;
	}
	SCE_DBG_PRINTF_LEVEL(1, 0, 130, "\t[compare value]: start %p, size 0x%x, seed 0x%08x\n",
			     start, size, seed);

	value = seed;
	block = (uint8_t *)start;
	for (addr = (uint8_t *)start, i = 0; addr < end; addr += step, i++) {
		value = MEMORY_TEST_NEXT(value);
		if (addr >= block + blockSize)
			block = addr;
		if (addr == block)
			SCE_DBG_PRINTF_LEVEL(1, 0, 141,
					     "\t\t(compare)   address %p, write value 0x%08x\n",
					     addr, value);
		switch ((value & 0x700) >> 8) {
		case 1:
			byte = addr[0];
			if (byte != (uint8_t)value) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 163,
						     "\t!!! memory test : byte access failed.\n");
				SCE_DBG_PRINTF_LEVEL(1, 0, 166,
						     "\t[%p]:0x%08x:0x%08x. [%p]:0x%02x != 0x%02x\n",
						     addr, *(uint32_t *)addr, value, addr, byte,
						     (uint8_t)value);
				SCE_DBG_ASSERT_LEVEL(0, 0, 167);
				for (;;)
					;
			}
			break;
		case 2:
			half = ((uint16_t *)addr)[1];
			if (half != (uint16_t)(value >> 16)) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 179,
						     "\t!!! memory test : half-word access failed.\n");
				SCE_DBG_PRINTF_LEVEL(1, 0, 182,
						     "\t[%p]:0x%08x:0x%08x. [%p]:0x%02x != 0x%02x\n",
						     (uint16_t *)start + i, *(uint32_t *)addr, value,
						     addr + 2, half, (uint16_t)(value >> 16));
				SCE_DBG_ASSERT_LEVEL(0, 0, 183);
				for (;;)
					;
			}
			break;
		case 3:
			byte = addr[1];
			if (byte != (uint8_t)(value >> 8)) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 196,
						     "\t!!! memory test : byte access failed.\n");
				SCE_DBG_PRINTF_LEVEL(1, 0, 199,
						     "\t[%p]:0x%08x:0x%08x. [%p]:0x%02x != 0x%02x\n",
						     addr, *(uint32_t *)addr, value, addr + 1, byte,
						     (uint8_t)(value >> 8));
				SCE_DBG_ASSERT_LEVEL(0, 0, 200);
				for (;;)
					;
			}
			break;
		case 5:
			/* Sony's test reports this byte but does not stop */
			byte = addr[2];
			if (byte != (uint8_t)(value >> 16)) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 213,
						     "\t!!! memory test : byte access failed.\n");
				SCE_DBG_PRINTF_LEVEL(1, 0, 216,
						     "\t[%p]:0x%08x:0x%08x. [%p]:0x%02x != 0x%02x\n",
						     addr, *(uint32_t *)addr, value, addr + 2, byte,
						     (uint8_t)(value >> 16));
			}
			break;
		case 6:
			half = ((uint16_t *)addr)[0];
			if (half != (uint16_t)value) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 228,
						     "\t!!! memory test : half-word access failed.\n");
				SCE_DBG_PRINTF_LEVEL(1, 0, 231,
						     "\t[%p]:0x%08x:0x%08x. [%p]:0x%02x != 0x%02x\n",
						     addr, *(uint32_t *)addr, value, addr, half,
						     (uint16_t)value);
				SCE_DBG_ASSERT_LEVEL(0, 0, 232);
				for (;;)
					;
			}
			break;
		case 7:
			byte = addr[3];
			if (byte != (uint8_t)(value >> 24)) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 245,
						     "\t!!! memory test : byte access failed.\n");
				SCE_DBG_PRINTF_LEVEL(1, 0, 248,
						     "\t[%p]:0x%08x:0x%08x. [%p]:0x%02x != 0x%02x\n",
						     addr, *(uint32_t *)addr, value, addr + 3, byte,
						     (uint8_t)(value >> 24));
				SCE_DBG_ASSERT_LEVEL(0, 0, 249);
				for (;;)
					;
			}
			break;
		case 0:
		case 4:
			word = *(uint32_t *)addr;
			if (word != value) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 151,
						     "\t!!! memory test : word access failed. [%p]:0x%08x != 0x%08x\n",
						     addr, word, value);
				SCE_DBG_ASSERT_LEVEL(0, 0, 152);
				for (;;)
					;
			}
			break;
		}
	}
	SCE_DBG_PRINTF_LEVEL(1, 0, 259, "\t[compare value]: end\n");
}
