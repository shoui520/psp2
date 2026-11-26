/*
 * SceBeisobmf C library wrappers over SceLibKernel sceClib functions.
 */

#include "be_isobmf_internal.h"
#include <libkernel/libkernel.h>
#include <stdarg.h>

void *beMemcpy(void *dst, const void *src, size_t size)
{
	return sceClibMemcpy_safe(dst, src, size);
}

void *beMemset(void *dst, int value, size_t size)
{
	return sceClibMemset(dst, value, size);
}

int beMemcmp(const void *lhs, const void *rhs, size_t size)
{
	return sceClibMemcmp(lhs, rhs, size);
}

int beSnprintf(char *buf, size_t size, const char *fmt, ...)
{
	va_list args;
	int ret;

	va_start(args, fmt);
	ret = sceClibVsnprintf(buf, size, fmt, args);
	va_end(args);
	return ret;
}

int beStrncmp(const char *lhs, const char *rhs, size_t maxLen)
{
	return sceClibStrncmp(lhs, rhs, maxLen);
}

char *beStrncpy(char *dst, const char *src, size_t maxLen)
{
	return sceClibStrncpy(dst, src, maxLen);
}

size_t beStrlen(const char *str)
{
	return sceClibStrnlen(str, 0x240);
}

int64_t beStrtoll(const char *nptr, char **endptr, int base)
{
	const char *cursor = nptr;
	int ch;
	int negative = 0;

	do {
		ch = static_cast<unsigned char>(*cursor++);
	} while ((sceClibLookCtypeTable(ch) & 0x08) != 0);

	if (ch == '-') {
		negative = 1;
		ch = *cursor++;
	} else if (ch == '+') {
		ch = *cursor++;
	}

	if ((base == 0 || base == 16) && ch == '0' && (*cursor == 'x' || *cursor == 'X')) {
		ch = cursor[1];
		cursor += 2;
		base = 16;
	} else if (base == 0) {
		base = (ch == '0') ? 8 : 10;
	}

	int64_t cutoff = negative ? (-9223372036854775807LL - 1LL) : 9223372036854775807LL;
	int cutlim = static_cast<int>(cutoff % base);
	cutoff /= base;
	if (negative) {
		if (cutlim > 0) {
			cutlim -= base;
			cutoff += 1;
		}
		cutlim = -cutlim;
	}

	int64_t acc = 0;
	int any = 0;
	for (;;) {
		int digit;
		if ((sceClibLookCtypeTable(ch) & 0x04) != 0) {
			digit = ch - '0';
		} else if ((sceClibLookCtypeTable(ch) & 0x03) != 0) {
			digit = ch - ((sceClibLookCtypeTable(ch) & 0x01) != 0 ? ('A' - 10) : ('a' - 10));
		} else {
			break;
		}

		if (digit >= base)
			break;

		if (any >= 0) {
			if (negative) {
				if (acc < cutoff || (acc == cutoff && digit > cutlim)) {
					any = -1;
					acc = -9223372036854775807LL - 1LL;
				} else {
					any = 1;
					acc = acc * base - digit;
				}
			} else {
				if (acc > cutoff || (acc == cutoff && digit > cutlim)) {
					any = -1;
					acc = 9223372036854775807LL;
				} else {
					any = 1;
					acc = acc * base + digit;
				}
			}
		}

		ch = static_cast<unsigned char>(*cursor++);
	}

	if (endptr != NULL)
		*endptr = const_cast<char *>(any != 0 ? (cursor - 1) : nptr);

	return acc;
}
