/* Shared Sysclib strings preserve native address-boundary behavior. */
#include <stddef.h>
#include <stdint.h>

/* Define SYSCLIB_SELECT_STRINGS and SYSCLIB_WANT_<NAME> for a shared subset. */
extern size_t strlen(const char *string);
extern size_t strnlen(const char *string, size_t count);

typedef uint32_t StringWord __attribute__((aligned(1), may_alias));
extern void *memset(void *destination, int value, size_t size);
extern void *memcpy(void *destination, const void *source, size_t size);

#if !defined(SYSCLIB_SELECT_STRINGS) || defined(SYSCLIB_WANT_STRCMP)
int strcmp(const char *left, const char *right)
{
	const unsigned char *a = (const unsigned char *)left;
	const unsigned char *start = (const unsigned char *)right;
	const unsigned char *b = start;
	int difference;

	do {
		unsigned int first = a[b - start];
		unsigned int second = *b++;

		if (second == 0)
			return first;
		difference = first - second;
	} while (difference == 0);
	return difference;
}
#endif

/* Name unknown. Equivalent to saturated byte subtraction of word from ones. */
static inline __attribute__((always_inline)) uint32_t ZeroByteMask(uint32_t word)
{
	uint32_t high = ((word & 0x7f7f7f7fU) + 0x7f7f7f7fU) |
			word | 0x7f7f7f7fU;

	return (~high & 0x80808080U) >> 7;
}

/* ARM CLZ(0) is32; the equivalent C builtin needs an explicit zero case. */
static inline __attribute__((always_inline)) unsigned FirstMatchingByte(uint32_t mask)
{
	return mask ? __builtin_clz(__builtin_bswap32(mask)) / 8 : 4;
}

static inline __attribute__((always_inline)) uint32_t PrefixByteMask(uintptr_t start)
{
	unsigned prefix = start & 3;

	return prefix ? 0x01010101U >> ((4 - prefix) * 8) : 0;
}

#if !defined(SYSCLIB_SELECT_STRINGS) || defined(SYSCLIB_WANT_STRNCMP)
int strncmp(const char *left, const char *right, size_t count)
{
	uintptr_t a = (uintptr_t)left;
	uintptr_t b = (uintptr_t)right;
	uintptr_t last;
	unsigned int x, y;

	if (!count)
		return 0;
	last = a + count;
	if (last < a)
		last = 0;
	last--;
	do {
		x = *(const unsigned char *)a++;
		y = *(const unsigned char *)b++;
		if (last < a || x == 0)
			break;
	} while (x == y);
	return (int)x - y;
}
#endif

#if !defined(SYSCLIB_SELECT_STRINGS) || defined(SYSCLIB_WANT_STRNLEN)
size_t strnlen(const char *string, size_t count)
{
	uintptr_t start = (uintptr_t)string;
	uintptr_t next = start & ~(uintptr_t)3;
	uintptr_t end, found;
	uint32_t word, mask;

	if (!count)
		return 0;
	word = *(const volatile uint32_t *)next;
	next += 4;
	end = start + count;
	if (end < start)
		end = UINT32_MAX;
	mask = ZeroByteMask(word | PrefixByteMask(start));
	while (next < end && !mask) {
		word = *(const volatile uint32_t *)next;
		next += 4;
		mask = ZeroByteMask(word);
	}
	found = next - 4 + FirstMatchingByte(mask);
	return (found <= end ? found : end) - start;
}
#endif

#if !defined(SYSCLIB_SELECT_STRINGS) || defined(SYSCLIB_WANT_STRLEN)
size_t strlen(const char *string)
{
	return strnlen(string, UINT32_MAX);
}
#endif

#if !defined(SYSCLIB_SELECT_STRINGS) || defined(SYSCLIB_WANT_MEMCHR)
void *memchr(const void *buffer, int value, size_t count)
{
	uintptr_t start = (uintptr_t)buffer;
	uintptr_t next = start & ~(uintptr_t)3;
	uintptr_t end, found;
	uint32_t needle = (uint32_t)(uint8_t)value * 0x01010101U;
	uint32_t word, mask;

	if (!count)
		return NULL;
	word = *(const volatile uint32_t *)next;
	next += 4;
	end = start + count;
	if (end < start)
		end = UINT32_MAX;
	mask = ZeroByteMask((word ^ needle) | PrefixByteMask(start));
	while (next < end && !mask) {
		word = *(const volatile uint32_t *)next;
		next += 4;
		mask = ZeroByteMask(word ^ needle);
	}
	found = next - 4 + FirstMatchingByte(mask);
	return found < end ? (void *)found : NULL;
}
#endif

#if !defined(SYSCLIB_SELECT_STRINGS) || defined(SYSCLIB_WANT_STRCHR)
char *strchr(const char *string, int value)
{
	uintptr_t start = (uintptr_t)string;
	uintptr_t next = start & ~(uintptr_t)3;
	uint32_t needle = (uint32_t)(uint8_t)value * 0x01010101U;
	uint32_t prefix = PrefixByteMask(start);
	uint32_t word = *(const volatile uint32_t *)next;
	uint32_t zeros = ZeroByteMask(word | prefix);
	uint32_t matches = ZeroByteMask((word ^ needle) | prefix);

	next += 4;
	while (!zeros && !matches) {
		word = *(const volatile uint32_t *)next;
		next += 4;
		zeros = ZeroByteMask(word);
		matches = ZeroByteMask(word ^ needle);
	}
	if (zeros)
		matches &= (zeros - 1) | zeros;
	if (!matches)
		return NULL;
	return (char *)(next - 4 + FirstMatchingByte(matches));
}
#endif

#if !defined(SYSCLIB_SELECT_STRINGS) || defined(SYSCLIB_WANT_STRRCHR)
char *strrchr(const char *string, int value)
{
	uintptr_t start = (uintptr_t)string;
	uintptr_t next = start & ~(uintptr_t)3;
	uintptr_t lastPlusOne = 1;
	uint32_t needle = (uint32_t)(uint8_t)value * 0x01010101U;
	uint32_t prefix = PrefixByteMask(start);
	uint32_t word = *(const volatile uint32_t *)next;
	uint32_t zeros = ZeroByteMask(word | prefix);
	uint32_t matches = ZeroByteMask((word ^ needle) | prefix);

	next += 4;
	while (!zeros) {
		if (matches)
			lastPlusOne = next - __builtin_clz(matches) / 8;
		word = *(const volatile uint32_t *)next;
		next += 4;
		zeros = ZeroByteMask(word);
		matches = ZeroByteMask(word ^ needle);
	}
	if (!needle)
		return (char *)(next - 4 + FirstMatchingByte(zeros));
	matches &= zeros - 1;
	if (matches)
		lastPlusOne = next - __builtin_clz(matches) / 8;
	return (char *)(lastPlusOne - 1);
}
#endif

#if !defined(SYSCLIB_SELECT_STRINGS) || defined(SYSCLIB_WANT_STRNCPY)
char *strncpy(char *destination, const char *source, size_t count)
{
	char *result = destination;
	uintptr_t src = (uintptr_t)source;

	if (!count)
		return result;
	while (src & 3) {
		unsigned char byte = *(const volatile unsigned char *)src++;

		if (!byte)
			goto pad;
		*(volatile unsigned char *)destination++ = byte;
		if (!--count)
			return result;
	}
	for (;;) {
		uint32_t word = *(const volatile uint32_t *)src;

		src += 4;
		/* Native code reads one word ahead even after an exact-word limit. */
		if (count >= 4 && !ZeroByteMask(word)) {
			*(StringWord *)destination = word;
			destination += 4;
			count -= 4;
			continue;
		}
		while (count) {
			if (!(word & 0xff))
				goto pad;
			*(volatile unsigned char *)destination++ = word;
			word >>= 8;
			count--;
		}
		return result;
	}
pad:
	memset(destination, 0, count);
	return result;
}
#endif

#if !defined(SYSCLIB_SELECT_STRINGS) || defined(SYSCLIB_WANT_STRLCPY)
#include "bsd_strings.c"
#endif

#if !defined(SYSCLIB_SELECT_STRINGS) || defined(SYSCLIB_WANT_STRLCAT)
size_t strlcat(char *destination, const char *source, size_t capacity)
{
	const char *start = source;
	size_t used = 0;

	while (used < capacity && destination[used])
		used++;
	if (used == capacity)
		return used + strlen(source);
	destination += used;
	capacity -= used;
	while (*source) {
		if (capacity > 1) {
			*destination++ = *source;
			capacity--;
		}
		source++;
	}
	*destination = 0;
	return used + (source - start);
}
#endif

/* Kernel FE39AEAC takes three words and returns the destination pointer. */
#if !defined(SYSCLIB_SELECT_STRINGS) || defined(SYSCLIB_WANT_STRNCPY_S)
char *strncpy_s(char *destination, const char *source, size_t limit)
{
	size_t length = strnlen(source, limit);

	/* Native terminator precedes the copy, including aliased buffers. */
	if (length < limit)
		destination[length] = '\0';
	if (length)
		memcpy(destination, source, length);
	return destination;
}
#endif
