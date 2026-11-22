/*
 * Small conversions: date and time comparison, and integers to and from
 * decimal and hexadecimal text (0x81052944-). The file's name is ours.
 */

#include <limits.h>

#include "mln_internal.h"

#define MLN_IS_DIGIT(c)	((unsigned int)((unsigned char)(c) - '0') <= 9)

/* the digits of each base, decimal (1) and hexadecimal (2) */
static const char s_digits[2][16] = {
	"0123456789",
	"0123456789abcdef",
};

/* compares two dates and times to the second: 1 if a is later, -1 if earlier, else 0 */
int MlnImpl_81052944(const MlnDateTime *a, const MlnDateTime *b)
{
	MlnDateTime x = *a;
	MlnDateTime y = *b;

	if (x.year > y.year)
		return 1;
	if (x.year < y.year)
		return -1;
	if (x.month > y.month)
		return 1;
	if (x.month < y.month)
		return -1;
	if (x.day > y.day)
		return 1;
	if (x.day < y.day)
		return -1;
	if (x.hour > y.hour)
		return 1;
	if (x.hour < y.hour)
		return -1;
	if (x.minute > y.minute)
		return 1;
	if (x.minute < y.minute)
		return -1;
	if (x.second > y.second)
		return 1;
	if (x.second < y.second)
		return -1;
	return 0;
}

/* writes a value in a base as exactly size digits, zero-padded, without a terminator */
int MlnImpl_810529E8(int value, int base, int size, char *buf)
{
	int count;
	int negative;

	switch (base) {
	case 1:
		count = MlnImpl_81052CA8(value, 1);
		if (size < 0 || size < count)
			return 3;
		negative = 0;
		if (value < 0) {
			value = -value;
			buf[0] = '-';
			negative = 1;
		}
		while (value > 9 && size > 1) {
			buf[--size] = s_digits[0][value % 10];
			value /= 10;
		}
		buf[size - 1] = s_digits[0][value % 10];
		MlnMemset(buf + negative, '0', size - 1 - negative);
		return 0;
	case 2:
		if (value < 0)
			return 3;
		count = MlnImpl_81052CA8(value, 2);
		if (size < 0 || size < count)
			return 3;
		while (value > 15 && size > 1) {
			buf[--size] = s_digits[1][value & 15];
			value >>= 4;
		}
		buf[size - 1] = s_digits[1][value & 15];
		MlnMemset(buf, '0', size - 1);
		return 0;
	}
	return 3;
}

/* reads a decimal integer after spaces and a sign, saturating at INT_MAX and -INT_MAX */
int MlnImpl_81052BD0(const char *s)
{
	int sign = 1;
	int limit = INT_MAX;
	int value = 0;
	int digit;
	signed char c;

	c = *s;
	while (c == ' ')
		c = *++s;
	if (c == '-') {
		sign = -1;
		limit = -INT_MAX;
		c = *++s;
	} else if (c == '+') {
		c = *++s;
	}
	if (c == 0)
		return 0;
	if (!MLN_IS_DIGIT(c))
		return 0;
	digit = c - '0';
	for (;;) {
		value = value * 10 + digit;
		c = *++s;
		if (c == 0 || !MLN_IS_DIGIT(c))
			return value * sign;
		digit = c - '0';
		if (digit > INT_MAX - value * 10)
			return limit;
	}
}

/* how many characters a value takes in a base, its sign included; -1 for another base */
int MlnImpl_81052CA8(int value, int base)
{
	int count;

	if (value < 0)
		count = 1;
	else
		count = 0;
	value = value < 0 ? -value : value;
	switch (base) {
	case 1:
		do {
			count++;
			value /= 10;
		} while (value > 0);
		return count;
	case 2:
		do {
			count++;
			value /= 16;
		} while (value > 0);
		return count;
	}
	return -1;
}
