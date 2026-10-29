#include "stdlib.h"
#include "ctype.h"

/* Sony's PAF random state starts at one in all three firmware variants. */
static uint32_t paf_random_seed = 1;

void sce_paf_srand(unsigned int seed)
{
	paf_random_seed = seed;
}

int sce_paf_rand(void)
{
	paf_random_seed = paf_random_seed * 0x41C64E6DU + 0x3039U;
	return (paf_random_seed >> 16) & 0x7fff;
}

void *sce_paf_bsearch(const void *key, const void *base, size_t count,
		      size_t element_size,
		      int (*compare)(const void *element, const void *key))
{
	int32_t first = 0;
	int32_t last;

	if (!key || !compare || !count || !element_size)
		return NULL;
	last = (int32_t)((uint32_t)count - 1);
	while (first <= last) {
		/* The native midpoint uses wrapped signed addition, rounded to zero. */
		int32_t middle = (int32_t)((uint32_t)first + (uint32_t)last) / 2;
		const void *element = (const void *)((uintptr_t)base +
			(uint32_t)middle * (uint32_t)element_size);
		int comparison = compare(element, key);

		if (!comparison)
			return (void *)element;
		if (comparison > 0)
			last = (int32_t)((uint32_t)middle - 1);
		else
			first = (int32_t)((uint32_t)middle + 1);
	}
	return NULL;
}

/* The native atoi/atol wrappers call this export rather than inlining it. */
__attribute__((noinline))
long sce_paf_strtol(const char *str, char **endptr, int base)
{
	const volatile signed char *cursor = (const volatile signed char *)str;
	const volatile signed char *next;
	uint32_t result = 0;
	int sign = 1;
	int ch;

	if (!cursor)
		return 0;
	ch = *cursor;
	next = cursor + 1;
	while (sce_paf_look_ctype_table(ch) & 8) {
		cursor = next;
		ch = *cursor;
		next = cursor + 1;
	}
	ch = *cursor;
	while (ch == '-' || ch == '+') {
		if (ch == '-')
			sign = -sign;
		cursor = next;
		ch = *cursor;
		next = cursor + 1;
	}
	if (base < 2 || base > 36)
		base = 0;
	if (ch == '0') {
		int prefix;

		cursor = next;
		prefix = *cursor;
		next = cursor + 1;
		if ((prefix == 'x' || prefix == 'X') &&
		    (base == 0 || base == 16)) {
			cursor = next;
			next = cursor + 1;
			base = 16;
		} else if ((prefix == 'b' || prefix == 'B') &&
			   (base == 0 || base == 2)) {
			cursor = next;
			next = cursor + 1;
			base = 2;
		} else if (!base) {
			base = 8;
		}
	} else if (sce_paf_toupper(ch) == 'O') {
		cursor = next;
		next = cursor + 1;
		base = 8;
	} else if (!base) {
		base = 10;
	}

	for (;;) {
		int digit;

		ch = *cursor;
		next = cursor + 1;
		if (sce_paf_look_ctype_table(ch) & 4) {
			digit = ch - '0';
		} else {
			digit = 9999999;
			if (sce_paf_look_ctype_table(ch) & 3)
				digit = sce_paf_tolower(ch) - 'a' + 10;
		}
		if (digit >= base)
			break;
		result = result * (uint32_t)base + (uint32_t)digit;
		cursor = next;
	}
	if (endptr)
		*endptr = (char *)cursor;
	return (long)(int32_t)(result * (uint32_t)sign);
}

unsigned long sce_paf_strtoul(const char *str, char **endptr, int base)
{
	const volatile signed char *cursor = (const volatile signed char *)str;
	const volatile signed char *next;
	uint32_t result = 0;
	int ch;

	if (!cursor)
		return 0;
	ch = *cursor;
	next = cursor + 1;
	while (sce_paf_look_ctype_table(ch) & 8) {
		cursor = next;
		ch = *cursor;
		next = cursor + 1;
	}
	ch = *cursor;
	if (base < 2 || base > 36)
		base = 0;
	if (ch == '0') {
		int prefix;

		cursor = next;
		prefix = *cursor;
		next = cursor + 1;
		if ((prefix == 'x' || prefix == 'X') &&
		    (base == 0 || base == 16)) {
			cursor = next;
			next = cursor + 1;
			base = 16;
		} else if ((prefix == 'b' || prefix == 'B') &&
			   (base == 0 || base == 2)) {
			cursor = next;
			next = cursor + 1;
			base = 2;
		} else if (!base) {
			base = 8;
		}
	} else if (sce_paf_toupper(ch) == 'O') {
		cursor = next;
		next = cursor + 1;
		base = 8;
	} else if (!base) {
		base = 10;
	}

	for (;;) {
		uint32_t digit;

		ch = *cursor;
		next = cursor + 1;
		if (sce_paf_look_ctype_table(ch) & 4) {
			digit = (uint32_t)(ch - '0');
		} else {
			digit = 9999999;
			if (sce_paf_look_ctype_table(ch) & 3)
				digit = (uint32_t)(sce_paf_tolower(ch) - 'a' + 10);
		}
		if (digit >= (uint32_t)base)
			break;
		result = result * (uint32_t)base + digit;
		cursor = next;
	}
	if (endptr)
		*endptr = (char *)cursor;
	return result;
}

long long sce_paf_strtoll(const char *str, char **endptr, int base)
{
	const volatile signed char *cursor = (const volatile signed char *)str;
	const volatile signed char *next;
	uint64_t result = 0;
	int sign = 1;
	int ch;

	if (!cursor)
		return 0;
	ch = *cursor;
	next = cursor + 1;
	while (sce_paf_look_ctype_table(ch) & 8) {
		cursor = next;
		ch = *cursor;
		next = cursor + 1;
	}
	ch = *cursor;
	while (ch == '-' || ch == '+') {
		if (ch == '-')
			sign = -sign;
		cursor = next;
		ch = *cursor;
		next = cursor + 1;
	}
	if (base < 2 || base > 36)
		base = 0;
	if (ch == '0') {
		int prefix;

		cursor = next;
		prefix = *cursor;
		next = cursor + 1;
		if ((prefix == 'x' || prefix == 'X') &&
		    (base == 0 || base == 16)) {
			cursor = next;
			next = cursor + 1;
			base = 16;
		} else if ((prefix == 'b' || prefix == 'B') &&
			   (base == 0 || base == 2)) {
			cursor = next;
			next = cursor + 1;
			base = 2;
		} else if (!base) {
			base = 8;
		}
	} else if (sce_paf_toupper(ch) == 'O') {
		cursor = next;
		next = cursor + 1;
		base = 8;
	} else if (!base) {
		base = 10;
	}
	for (;;) {
		uint32_t digit;

		ch = *cursor;
		next = cursor + 1;
		if (sce_paf_look_ctype_table(ch) & 4) {
			digit = (uint32_t)(ch - '0');
		} else {
			digit = 9999999;
			if (sce_paf_look_ctype_table(ch) & 3)
				digit = (uint32_t)(sce_paf_tolower(ch) - 'a' + 10);
		}
		if (digit >= (uint32_t)base)
			break;
		result = result * (uint64_t)(uint32_t)base + digit;
		cursor = next;
	}
	if (endptr)
		*endptr = (char *)cursor;
	return (long long)(int64_t)(result * (uint64_t)sign);
}

unsigned long long sce_paf_strtoull(const char *str, char **endptr, int base)
{
	const volatile signed char *cursor = (const volatile signed char *)str;
	const volatile signed char *next;
	uint64_t result = 0;
	int ch;

	if (!cursor)
		return 0;
	ch = *cursor;
	next = cursor + 1;
	while (sce_paf_look_ctype_table(ch) & 8) {
		cursor = next;
		ch = *cursor;
		next = cursor + 1;
	}
	ch = *cursor;
	if (base < 2 || base > 36)
		base = 0;
	if (ch == '0') {
		int prefix;

		cursor = next;
		prefix = *cursor;
		next = cursor + 1;
		if ((prefix == 'x' || prefix == 'X') &&
		    (base == 0 || base == 16)) {
			cursor = next;
			next = cursor + 1;
			base = 16;
		} else if ((prefix == 'b' || prefix == 'B') &&
			   (base == 0 || base == 2)) {
			cursor = next;
			next = cursor + 1;
			base = 2;
		} else if (!base) {
			base = 8;
		}
	} else if (sce_paf_toupper(ch) == 'O') {
		cursor = next;
		next = cursor + 1;
		base = 8;
	} else if (!base) {
		base = 10;
	}
	for (;;) {
		uint32_t digit;

		ch = *cursor;
		next = cursor + 1;
		if (sce_paf_look_ctype_table(ch) & 4) {
			digit = (uint32_t)(ch - '0');
		} else {
			digit = 9999999;
			if (sce_paf_look_ctype_table(ch) & 3)
				digit = (uint32_t)(sce_paf_tolower(ch) - 'a' + 10);
		}
		if (digit >= (uint32_t)base)
			break;
		result = result * (uint64_t)(uint32_t)base + digit;
		cursor = next;
	}
	if (endptr)
		*endptr = (char *)cursor;
	return result;
}

int sce_paf_atoi(const char *str)
{
	return (int)sce_paf_strtol(str, NULL, 10);
}

long sce_paf_atol(const char *str)
{
	return sce_paf_strtol(str, NULL, 10);
}

int sce_paf_abs(int value)
{
	if (value < 0)
		return (int)(0U - (uint32_t)value);
	return value;
}

int ScePafStdc_D41FD265(void)
{
	return 0;
}
