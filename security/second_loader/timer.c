/*
 * timer.c: MeP control-bus hardware timer microsecond delay and endianness
 * conversion helpers (0x805168..0x805303).
 *
 * Function and variable names here are ours (descriptive).
 */

#include "second_loader.h"

/* ============================================================================
 * .rodata (0x80c060..0x80c077)
 * ============================================================================ */

static const uint32_t s_cmepClockFreqHz[6] = {
	27000000u,
	42000000u,
	56000000u,
	83000000u,
	111000000u,
	166000000u,
};

/* ============================================================================
 * Functions (0x805168..0x805303)
 * ============================================================================ */

/* 0x805168 */
uint32_t cmepGetClockFrequencyHz(void)
{
	uint32_t step = (PERVASIVE_BASECLK->cmepClock & 0xfu) - 2u;

	if (step < 6u)
		return s_cmepClockFreqHz[step];
	return 50000000u;
}

/* 0x805194 */
void cmepDelayTicks(uint32_t ticks)
{
	uint32_t tis;

	if (ticks == 0u)
		return;

	__asm__ volatile(
		"stcb %1, 0x403\n\t"
		"stcb %1, 0x405\n\t"
		"stcb %2, 0x401\n\t"
		"stcb %3, 0x400\n\t"
		"stcb %1, 0x404\n\t"
		"stcb %3, 0x402\n\t"
		"1:\n\t"
		"ldcb %0, 0x404\n\t"
		"and3 %0, %0, 1\n\t"
		"beqz %0, 1b\n\t"
		"stcb %1, 0x404\n\t"
		"stcb %1, 0x402"
		: "=&r"(tis)
		: "r"(0u), "r"(ticks), "r"(1u)
	);
}

/* 0x8051d0 */
void cmepDelayUs(uint32_t microseconds)
{
	uint32_t ticksPerUs = cmepGetClockFrequencyHz() / 1000000u;

	cmepDelayTicks(ticksPerUs * microseconds);
}

/* 0x8051fc */
uint16_t htole16Sl(uint16_t val)
{
	return val;
}

/* 0x805202 */
uint16_t le16tohSl(uint16_t val)
{
	return val;
}

/* 0x805208 */
uint32_t htole32Sl(uint32_t val)
{
	return val;
}

/* 0x80520c */
uint32_t le32tohSl(uint32_t val)
{
	return val;
}

/* 0x805210 */
uint64_t htole64Sl(uint64_t val)
{
	return val;
}

/* 0x805220 */
void writeLe64(uint64_t *out, const uint64_t *in)
{
	*out = *in;
}

/* 0x805230 */
int16_t bswap16(uint16_t val)
{
	uint8_t bytes[2];

	bytes[0] = (uint8_t)(val >> 8);
	bytes[1] = (uint8_t)val;
	return *(int16_t *)bytes;
}

/* 0x805248 */
int16_t htobe16Sl(uint16_t val)
{
	return bswap16(val);
}

/* 0x80524c */
int16_t be16tohSl(uint16_t val)
{
	return bswap16(val);
}

/* 0x805250 */
uint32_t bswap32(uint32_t val)
{
	uint8_t bytes[4];

	bytes[0] = (uint8_t)(val >> 24);
	bytes[1] = (uint8_t)(val >> 16);
	bytes[2] = (uint8_t)(val >> 8);
	bytes[3] = (uint8_t)val;
	return *(uint32_t *)bytes;
}

/* 0x805272 */
uint32_t htobe32Sl(uint32_t val)
{
	return bswap32(val);
}

/* 0x805276 */
uint32_t be32tohSl(uint32_t val)
{
	return bswap32(val);
}

/* 0x80527a */
uint64_t bswap64(uint64_t val)
{
	union {
		uint64_t u64;
		uint8_t b[8];
	} u;
	unsigned int i;

	u.u64 = val;
	for (i = 0; i < 4u; i++) {
		uint8_t tmp = u.b[i];

		u.b[i] = u.b[7u - i];
		u.b[7u - i] = tmp;
	}
	return u.u64;
}

/* 0x8052a4 */
uint64_t htobe64Sl(uint64_t val)
{
	return bswap64(val);
}

/* 0x8052d2 */
uint64_t be64tohSl(uint64_t val)
{
	return bswap64(val);
}

/* 0x805300 */
int timerNoopHook(void)
{
	return 0;
}
