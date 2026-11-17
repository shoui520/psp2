/*
 * SceNpCommonPs4: string to number conversions (the library's own strtol, strtoull and the checked
 * ones over sceClibStrtoll).
 */

#include "np_common_ps4_internal.h"

extern "C" {

#define CTYPE_SPACE	8
#define CTYPE_DIGIT	4
#define CTYPE_ALPHA	3

int SceNpCommonPs4_32155BF8(const char *str, const char **pEnd, int base)
{
	const char *p;
	const char *next;
	char c;
	int sign;
	int digit;
	int value;
	int ch;

	p = str;
	value = 0;
	if (str == NULL)
		return 0;
	c = *str;
	next = str + 1;
	while ((sceClibLookCtypeTable(c) & CTYPE_SPACE) != 0) {
		p = next;
		c = *next++;
	}
	ch = *p;
	sign = 1;
	while (ch == '-' || ch == '+') {
		if (ch == '-')
			sign = -sign;
		p = next;
		ch = *next++;
	}
	if (base < 2 || base > 36)
		base = 0;
	if (ch == '0') {
		const char *prefix;

		p = next;
		ch = *next;
		prefix = next++;
		if (ch == 'x')
			goto hex;
		if (ch == 'b')
			goto binary;
		if (ch == 'X')
			goto hex;
		if (ch == 'B')
			goto binary;
		goto octal;
hex:
		if (base != 0 && base != 16)
			goto decimal;
		p = next;
		next = prefix + 2;
		base = 16;
		goto digits;
binary:
		if (base != 0 && base != 2)
			goto decimal;
		p = next;
		next = prefix + 2;
		base = 2;
		goto digits;
octal:
		if (base == 0)
			base = 8;
		goto digits;
	}
	if (sceClibToupper(ch) == 'O') {
		p = next;
		next = p + 1;
		base = 8;
		goto digits;
	}
decimal:
	if (base == 0)
		base = 10;
	for (;; value = base * value + digit) {
digits:
		c = *p;
		p = next;
		ch = c;
		if ((sceClibLookCtypeTable(c) & CTYPE_DIGIT) != 0) {
			digit = ch - '0';
		} else {
			digit = 9999999;
			if ((sceClibLookCtypeTable(ch) & CTYPE_ALPHA) != 0)
				digit = sceClibTolower(ch) - 87;
		}
		if (digit >= base)
			break;
		next = p + 1;
	}
	if (pEnd != NULL)
		*pEnd = p - 1;
	return value * sign;
}

SceUInt64 SceNpCommonPs4_80F6D82E(const char *str, const char **pEnd, int base)
{
	const char *p;
	const char *next;
	char c;
	SceUInt32 digit;
	SceInt64 value;
	int ch;

	p = str;
	value = 0;
	if (str == NULL)
		return 0;
	c = *str;
	next = str + 1;
	while ((sceClibLookCtypeTable(c) & CTYPE_SPACE) != 0) {
		p = next;
		c = *next++;
	}
	ch = *p;
	if (base < 2 || base > 36)
		base = 0;
	if (ch == '0') {
		const char *prefix;

		p = next;
		ch = *next;
		prefix = next++;
		if (ch == 'x')
			goto hex;
		if (ch == 'b')
			goto binary;
		if (ch == 'X')
			goto hex;
		if (ch == 'B')
			goto binary;
		goto octal;
hex:
		if (base != 0 && base != 16)
			goto decimal;
		p = next;
		next = prefix + 2;
		base = 16;
		goto digits;
binary:
		if (base != 0 && base != 2)
			goto decimal;
		p = next;
		next = prefix + 2;
		base = 2;
		goto digits;
octal:
		if (base == 0)
			base = 8;
		goto digits;
	}
	if (sceClibToupper(ch) == 'O') {
		p = next;
		next = p + 1;
		base = 8;
		goto digits;
	}
decimal:
	if (base == 0)
		base = 10;
	for (;; value = base * value + digit) {
digits:
		c = *p;
		p = next;
		ch = c;
		if ((sceClibLookCtypeTable(c) & CTYPE_DIGIT) != 0) {
			digit = ch - '0';
		} else {
			digit = 9999999;
			if ((sceClibLookCtypeTable(ch) & CTYPE_ALPHA) != 0)
				digit = sceClibTolower(ch) - 87;
		}
		if (digit >= base)
			break;
		next = p + 1;
	}
	if (pEnd != NULL)
		*pEnd = p - 1;
	return value;
}

int SceNpCommonPs4_3AE5ABF3(const char *str, SceInt32 *pValue, int base, SceSize *pLength)
{
	const char *end;
	SceInt64 value;

	value = sceClibStrtoll(str, (char **)&end, base);
	if (end == str)
		return NP_COMMON_PS4_ERROR_80558008;
	if (value > 0x7FFFFFFF)
		return NP_COMMON_PS4_ERROR_80558008;
	if (value < -0x7FFFFFFF - 1)
		return NP_COMMON_PS4_ERROR_80558008;
	*pValue = (SceInt32)value;
	if (pLength != NULL)
		*pLength = end - str;
	return 0;
}

int SceNpCommonPs4_D5B30475(const char *str, SceUInt32 *pValue, int base, SceSize *pLength)
{
	const char *end;
	SceInt64 value;

	value = sceClibStrtoll(str, (char **)&end, base);
	if (end == str)
		return NP_COMMON_PS4_ERROR_80558008;
	if (value > 0xFFFFFFFFLL)
		return NP_COMMON_PS4_ERROR_80558008;
	*pValue = (SceUInt32)value;
	if (pLength != NULL)
		*pLength = end - str;
	return 0;
}

int NpCommonPs4StrToInt64(const char *str, SceInt64 *pValue, int base, SceSize *pLength)
{
	const char *end;
	SceInt64 value;

	value = sceClibStrtoll(str, (char **)&end, base);
	*pValue = value;
	if (end == str)
		return NP_COMMON_PS4_ERROR_80558008;
	if (value == 0x7FFFFFFFFFFFFFFFLL)
		return NP_COMMON_PS4_ERROR_80558008;
	if (value == (SceInt64)0x8000000000000000ULL)
		return NP_COMMON_PS4_ERROR_80558008;
	if (pLength != NULL)
		*pLength = end - str;
	return 0;
}

int NpCommonPs4StrToUInt64(const char *str, SceUInt64 *pValue, int base, SceSize *pLength)
{
	const char *end;
	SceUInt64 value;

	value = SceNpCommonPs4_80F6D82E(str, &end, base);
	*pValue = value;
	if (end == str)
		return NP_COMMON_PS4_ERROR_80558008;
	if (value == (SceUInt64)-1)
		return NP_COMMON_PS4_ERROR_80558008;
	if (pLength != NULL)
		*pLength = end - str;
	return 0;
}

}
