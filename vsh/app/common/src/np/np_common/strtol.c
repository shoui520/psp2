/*
 * SceNpCommon: number parsing in the manner of strtol, with Sony's prefixes: "0x" for 16,
 * "0b" for 2, a leading 0 for 8, and a leading 'o' or 'O' for 8 whatever the base asked for.
 * Also the communication ID "NPWR12345_00" read and written.
 */

#include "np_common_internal.h"

#define NP_IS_SPACE(c)	((sceClibLookCtypeTable(c) & SCE_CLIB_CTYPE_SPACE) != 0)
#define NP_IS_DIGIT(c)	((sceClibLookCtypeTable(c) & SCE_CLIB_CTYPE_DIGIT) != 0)
#define NP_IS_ALPHA(c)	((sceClibLookCtypeTable(c) & (SCE_CLIB_CTYPE_UPPER | SCE_CLIB_CTYPE_LOWER)) != 0)

/* the value of a digit character, 9999999 for anything else */
#define NP_DIGIT_NONE	9999999

int SceNpCommon_4C4AA451(const char *str, const char **end, int base)
{
	int value = 0, sign = 1, digit;
	const char *next;
	char c;

	if (str == NULL)
		return 0;
	next = str + 1;
	c = *str;
	while (NP_IS_SPACE(c)) {
		str = next;
		c = *next++;
	}
	c = *str;
	while (c == '-' || c == '+') {
		if (c == '-')
			sign = -sign;
		str = next;
		c = *next++;
	}
	if (base < 2 || base > 36)
		base = 0;
	if (c == '0') {
		str = next;
		c = *next++;
		if (c == 'x' || c == 'X') {
			if (base == 0 || base == 16) {
				str = next;
				next++;
				base = 16;
			}
		} else if (c == 'b' || c == 'B') {
			if (base == 0 || base == 2) {
				str = next;
				next++;
				base = 2;
			}
		} else if (base == 0) {
			base = 8;
		}
	} else if (sceClibToupper(c) == 'O') {
		str = next;
		next++;
		base = 8;
	} else if (base == 0) {
		base = 10;
	}
	for (;;) {
		c = *str;
		str = next;
		if (NP_IS_DIGIT(c)) {
			digit = c - '0';
		} else {
			digit = NP_DIGIT_NONE;
			if (NP_IS_ALPHA(c))
				digit = sceClibTolower(c) - 'a' + 10;
		}
		if (digit >= base)
			break;
		next = str + 1;
		value = value * base + digit;
	}
	if (end != NULL)
		*end = str - 1;
	return value * sign;
}

SceInt64 SceNpCommon_37954C1F(const char *str, const char **end, int base)
{
	SceInt64 value = 0, sign = 1, radix;
	unsigned int digit;
	const char *next;
	char c;

	if (str == NULL)
		return 0;
	next = str + 1;
	c = *str;
	while (NP_IS_SPACE(c)) {
		str = next;
		c = *next++;
	}
	c = *str;
	while (c == '-' || c == '+') {
		if (c == '-')
			sign = -sign;
		str = next;
		c = *next++;
	}
	if (base < 2 || base > 36)
		base = 0;
	if (c == '0') {
		str = next;
		c = *next++;
		if (c == 'x' || c == 'X') {
			if (base == 0 || base == 16) {
				str = next;
				next++;
				base = 16;
			}
		} else if (c == 'b' || c == 'B') {
			if (base == 0 || base == 2) {
				str = next;
				next++;
				base = 2;
			}
		} else if (base == 0) {
			base = 8;
		}
	} else if (sceClibToupper(c) == 'O') {
		str = next;
		next++;
		base = 8;
	} else if (base == 0) {
		base = 10;
	}
	radix = base;
	for (;;) {
		c = *str;
		str = next;
		if (NP_IS_DIGIT(c)) {
			digit = c - '0';
		} else {
			digit = NP_DIGIT_NONE;
			if (NP_IS_ALPHA(c))
				digit = sceClibTolower(c) - 'a' + 10;
		}
		if (digit >= radix)
			break;
		next = str + 1;
		value = value * radix + digit;
	}
	if (end != NULL)
		*end = str - 1;
	return value * sign;
}

unsigned int SceNpCommon_5B770B80(const char *str, const char **end, int base)
{
	unsigned int value = 0, digit;
	const char *next;
	char c;

	if (str == NULL)
		return 0;
	next = str + 1;
	c = *str;
	while (NP_IS_SPACE(c)) {
		str = next;
		c = *next++;
	}
	c = *str;
	if (base < 2 || base > 36)
		base = 0;
	if (c == '0') {
		str = next;
		c = *next++;
		if (c == 'x' || c == 'X') {
			if (base == 0 || base == 16) {
				str = next;
				next++;
				base = 16;
			}
		} else if (c == 'b' || c == 'B') {
			if (base == 0 || base == 2) {
				str = next;
				next++;
				base = 2;
			}
		} else if (base == 0) {
			base = 8;
		}
	} else if (sceClibToupper(c) == 'O') {
		str = next;
		next++;
		base = 8;
	} else if (base == 0) {
		base = 10;
	}
	for (;;) {
		c = *str;
		str = next;
		if (NP_IS_DIGIT(c)) {
			digit = c - '0';
		} else {
			digit = NP_DIGIT_NONE;
			if (NP_IS_ALPHA(c))
				digit = sceClibTolower(c) - 'a' + 10;
		}
		if (digit >= (unsigned int)base)
			break;
		next = str + 1;
		value = value * base + digit;
	}
	if (end != NULL)
		*end = str - 1;
	return value;
}

SceUInt64 SceNpCommon_CF843378(const char *str, const char **end, int base)
{
	SceUInt64 value = 0;
	unsigned int digit;
	const char *next;
	char c;

	if (str == NULL)
		return 0;
	next = str + 1;
	c = *str;
	while (NP_IS_SPACE(c)) {
		str = next;
		c = *next++;
	}
	c = *str;
	if (base < 2 || base > 36)
		base = 0;
	if (c == '0') {
		str = next;
		c = *next++;
		if (c == 'x' || c == 'X') {
			if (base == 0 || base == 16) {
				str = next;
				next++;
				base = 16;
			}
		} else if (c == 'b' || c == 'B') {
			if (base == 0 || base == 2) {
				str = next;
				next++;
				base = 2;
			}
		} else if (base == 0) {
			base = 8;
		}
	} else if (sceClibToupper(c) == 'O') {
		str = next;
		next++;
		base = 8;
	} else if (base == 0) {
		base = 10;
	}
	for (;;) {
		c = *str;
		str = next;
		if (NP_IS_DIGIT(c)) {
			digit = c - '0';
		} else {
			digit = NP_DIGIT_NONE;
			if (NP_IS_ALPHA(c))
				digit = sceClibTolower(c) - 'a' + 10;
		}
		if (digit >= (unsigned int)base)
			break;
		next = str + 1;
		value = value * base + digit;
	}
	if (end != NULL)
		*end = str - 1;
	return value;
}

int SceNpCommon_CC8E6AC6(const char *str)
{
	return SceNpCommon_4C4AA451(str, NULL, 10);
}

SceInt64 SceNpCommon_E1FF4E70(const char *str)
{
	return SceNpCommon_37954C1F(str, NULL, 0);
}

int SceNpCommon_5BC54899(SceNpCommunicationId *commId, const char *str)
{
	if (str == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (sceClibStrnlen(str, 0x7fffffff) > 12)
		return SCE_NP_UTIL_ERROR_INVALID_TITLEID;
	if (str[9] != '_')
		return SCE_NP_UTIL_ERROR_INVALID_TITLEID;
	sceClibMemset(commId, 0, sizeof(*commId));
	sceClibMemcpy_safe(commId->data, str, sizeof(commId->data));
	commId->num = (SceUChar8)SceNpCommon_4C4AA451(&str[10], NULL, 10);
	return 0;
}

/* writes value in decimal, right-aligned in its digits, when it has 1 to width digits */
static inline void npUintToString(char *str, unsigned int value, SceSize width)
{
	unsigned int rest = value;
	SceSize digits = 0;
	char *p;

	do {
		digits++;
		rest = (rest - rest % 10) / 10;
	} while (rest != 0);
	if (str == NULL || digits > width || digits == 0)
		return;
	p = &str[digits];
	do {
		p[-1] = value % 10 + '0';
		digits--;
		value = (value - value % 10) / 10;
		p--;
	} while (digits != 0);
}

int SceNpCommon_E426FEE8(const SceNpCommunicationId *commId, char *str, SceSize size)
{
	unsigned int num;

	if (commId == NULL || str == NULL)
		return SCE_NP_UTIL_ERROR_INVALID_ARGUMENT;
	if (size < 13)
		return SCE_NP_UTIL_ERROR_INSUFFICIENT;
	if (commId->num > 99)
		return SCE_NP_UTIL_ERROR_INVALID_TITLEID;
	sceClibMemcpy_safe(str, commId->data, sizeof(commId->data));
	str[9] = '_';
	num = commId->num;
	if (num >= 10) {
		npUintToString(&str[10], num, 2);
	} else {
		str[10] = '0';
		npUintToString(&str[11], commId->num, 1);
	}
	str[12] = '\0';
	return 0;
}
