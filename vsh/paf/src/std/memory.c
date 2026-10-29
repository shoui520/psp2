#include "../compiler.h"
#include "string.h"
#include <libkernel/libkernel.h>

/* Keep the checked-copy caller boundary separate from its string users. */
/* The native wmemcpy wrapper also calls this function out of line. */
__attribute__((noinline))
void *sce_paf_memcpy(void *dst, const void *src, size_t count)
{
	if (dst != src)
		sceClibMemcpy_safe(dst, src, count);
	return dst;
}

/* The native wide-memory wrapper calls this export out of line. */
__attribute__((noinline))
void *sce_paf_memmove(void *dst, const void *src, size_t count)
{
	if (dst != src)
		sceClibMemmove(dst, src, count);
	return dst;
}

void *sce_paf_memset(void *dst, int ch, size_t count)
{
	sceClibMemset(dst, ch, count);
	return dst;
}

void *sce_paf_bcopy(const void *src, void *dst, size_t count)
{
	if (dst != src)
		return sceClibMemmove(dst, src, count);
	return dst;
}

void *sce_paf_bzero(void *dst, size_t count)
{
	return sceClibMemset(dst, 0, count);
}

void *sce_paf_wmemcpy(void *destination, const void *source, size_t count)
{
	return sce_paf_memcpy(destination, source, count << 1);
}

void *sce_paf_wmemmove(void *destination, const void *source, size_t count)
{
	return sce_paf_memmove(destination, source, count << 1);
}

uint16_t *sce_paf_wmemset(uint16_t *destination, int value, size_t count)
{
	volatile uint16_t *cursor = (volatile uint16_t *)destination;
	int32_t remaining = (int32_t)count;

	while (remaining > 0) {
		*cursor++ = (uint16_t)value;
		remaining--;
	}
	return destination;
}

int sce_paf_wmemcmp(const uint16_t *left, const uint16_t *right, size_t count)
{
	const volatile uint16_t *leftByte = (const volatile uint16_t *)left;
	const volatile uint16_t *rightByte = (const volatile uint16_t *)right;
	int32_t remaining = (int32_t)count;

	while (remaining > 0) {
		int rightCh = *rightByte;
		int leftCh = *leftByte;

		if (leftCh != rightCh)
			return leftCh < rightCh ? -1 : 1;
		leftByte++;
		rightByte++;
		remaining--;
	}
	return 0;
}

int sce_paf_memcmp(const void *left, const void *right, size_t count)
{
	const volatile uint8_t *lhs = (const volatile uint8_t *)left;
	const volatile uint8_t *rhs = (const volatile uint8_t *)right;
	int32_t remaining = (int32_t)count;

	while (remaining > 0) {
		int rightByte = *rhs;
		int leftByte = *lhs;

		if (leftByte != rightByte)
			return leftByte < rightByte ? -1 : 1;
		lhs++;
		rhs++;
		remaining--;
	}
	return 0;
}

int sce_paf_bcmp(const void *left, const void *right, size_t count)
{
	const volatile uint8_t *lhs = (const volatile uint8_t *)left;
	const volatile uint8_t *rhs = (const volatile uint8_t *)right;
	int32_t remaining = (int32_t)count;

	while (remaining > 0) {
		int rightByte = *rhs;
		int leftByte = *lhs;

		if (leftByte != rightByte)
			return leftByte < rightByte ? -1 : 1;
		lhs++;
		rhs++;
		remaining--;
	}
	return 0;
}
